/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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

#include "sched/sge_resource_utilization.h"
#include "sched/sge_sharetree_printing.h"

#include "sgeobj/ocs_Category.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_usage.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_STU.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_CT.h"

#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sge_reporting_qmaster.h"
#include "sge_rusage.h"

#include "ocs_JsonReportingFileWriter.h"
#include "ocs_JsonUtil.h"

namespace ocs {
   bool JsonReportingFileWriter::create_acct_record(lList **answer_list, lListElem *job_report,
                                                    lListElem *job, lListElem *ja_task, bool intermediate) {
      DENTER(TOP_LAYER);

      // get the job category string
      const lList *master_category_list = *ocs::DataStore::get_master_list(SGE_TYPE_CATEGORY);
      uint32_t category_id = ocs::tom::get<JB_category_id>(job);
      ocs::tom::Element<ocs::tom::CT> category = ocs::tom::find_first_rw<CT_id>(master_category_list, category_id);
      const char *category_string = ocs::tom::get<CT_str>(category);

      // get accounting data
      rapidjson::StringBuffer json_buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(json_buffer);

      // Once an intermediate record has been written for this job, every further
      // record - including the final one - has to report the usage consumed since
      // the previous record, else the records of the job no longer partition its
      // usage and a consumer summing them up counts everything before the last
      // intermediate record twice.
      bool do_intermediate = intermediate_usage_written(job_report, ja_task) || intermediate;

      // we need to protect the usage_pattern_list via the config_mutex
      sge_mutex_lock(config_mutex_name.c_str(), __func__, __LINE__, &config_mutex);
      bool ret = sge_write_rusage(nullptr, &writer, job_report, job, ja_task, category_string, &usage_pattern_list, 0,
                             do_intermediate, true);
      sge_mutex_unlock(config_mutex_name.c_str(), __func__, __LINE__, &config_mutex);

      if (ret) {
         // append data to buffer
         json_buffer.Put('\n');
         sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
         buffer += json_buffer.GetString();
         sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
      }

      DRETURN(ret);
   }

   /**
    * @brief Emit a single JSONL online_usage record for the running job.
    *
    * When pe_task is non-null the record carries that pe_task's scaled
    * usage and a pe_taskid field. When pe_task is null the record carries
    * the ja_task's scaled usage; if aggregate_pe_tasks is set, scaled
    * usage of every pe_task in `ja_task->JAT_task_list` is summed into the
    * ja_task value (matching the convention used by sge_write_rusage when
    * the PE has `accounting_summary` set). Only the variables listed in
    * `reporting_params=online_usage=...` are emitted under `usage`;
    * variables missing from the scaled usage list are silently skipped
    * (no `null` field).
    *
    * If none of the configured variables have any data yet (e.g. right
    * after job start, before the execd has produced its first usage
    * report), no record is emitted at all — empty-`usage` records would
    * clutter the reporting file without adding information.
    *
    * @param answer_list receives error messages
    * @param job_report the job report the execution host sent
    * @param job the job (`JB_Type`)
    * @param ja_task the array task
    * @param pe_task the PE task, or nullptr for the ja_task's own usage
    * @param aggregate_pe_tasks whether the PE tasks are summed into the ja_task value
    * @return true on success
    *
    * @see ReportingFileWriter::create_online_usage_records()
    */
   bool
   JsonReportingFileWriter::create_online_usage_record(lList **answer_list, lListElem *job_report,
                                                       lListElem *job, lListElem *ja_task, lListElem *pe_task,
                                                       bool aggregate_pe_tasks) {
      DENTER(TOP_LAYER);

      if (job == nullptr || ja_task == nullptr) {
         DRETURN(true);
      }

      // take a snapshot of the configured variable names
      std::vector<std::string> vars;
      sge_mutex_lock(config_mutex_name.c_str(), __func__, __LINE__, &config_mutex);
      vars = online_usage_vars;
      sge_mutex_unlock(config_mutex_name.c_str(), __func__, __LINE__, &config_mutex);

      if (vars.empty()) {
         DRETURN(true);
      }

      // pe_task != nullptr: report scaled usage of the pe_task,
      // otherwise the (possibly aggregated) scaled usage on the ja_task
      const lList *primary_usage = (pe_task != nullptr)
                                   ? static_cast<const lList *>(ocs::tom::get<PET_scaled_usage>(pe_task))
                                   : static_cast<const lList *>(ocs::tom::get<JAT_scaled_usage_list>(ja_task));

      // aggregation only applies to ja_task-level records
      const bool aggregate = (pe_task == nullptr) && aggregate_pe_tasks;

      // Suppress empty-usage records: if none of the configured variables
      // have data yet, do not emit anything.
      bool any_data = false;
      for (const auto &name : vars) {
         if (static_cast<const lListElem *>(ocs::tom::find_first<UA_name>(primary_usage, name.c_str())) != nullptr) {
            any_data = true;
            break;
         }
         if (aggregate) {
            ocs::tom::ConstElement<ocs::tom::PET> pe_task_iter;
            for_each_ep(pe_task_iter, ocs::tom::get<JAT_task_list>(ja_task)) {
               if (static_cast<const lListElem *>(ocs::tom::find_first_in<PET_scaled_usage, UA_name>(pe_task_iter, name.c_str())) != nullptr) {
                  any_data = true;
                  break;
               }
            }
            if (any_data) {
               break;
            }
         }
      }
      if (!any_data) {
         DRETURN(true);
      }

      rapidjson::StringBuffer stringBuffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

      writer.StartObject();
      write_json(writer, "time", sge_get_gmt64());
      write_json(writer, "type", "online_usage");
      write_json(writer, "job_number", ocs::tom::get<JB_job_number>(job));
      int task_number = job_is_array(job) ? (int) ocs::tom::get<JAT_task_number>(ja_task) : 0;
      write_json(writer, "task_number", task_number);
      // Always emit pe_taskid so the JSONL record carries the full sge_job
      // primary key; "NONE" is the classic-format sentinel for ja_task-level
      // records (and for jobs without a parallel environment).
      write_json(writer, "pe_taskid", pe_task != nullptr ? ocs::tom::get<PET_id>(pe_task) : NONE_STR);

      writer.Key("usage");
      writer.StartObject();
      for (const auto &name : vars) {
         double total = 0.0;
         bool found_any = false;

         ocs::tom::ConstElement<ocs::tom::UA> u = ocs::tom::find_first<UA_name>(primary_usage, name.c_str());
         if (u != nullptr) {
            total = ocs::tom::get<UA_value>(u);
            found_any = true;
         }

         if (aggregate) {
            ocs::tom::ConstElement<ocs::tom::PET> pe_task_iter;
            for_each_ep(pe_task_iter, ocs::tom::get<JAT_task_list>(ja_task)) {
               ocs::tom::ConstElement<ocs::tom::UA> pu = ocs::tom::find_first_in<PET_scaled_usage, UA_name>(pe_task_iter, name.c_str());
               if (pu != nullptr) {
                  total += ocs::tom::get<UA_value>(pu);
                  found_any = true;
               }
            }
         }

         if (found_any) {
            write_json(writer, name.c_str(), total);
         }
      }
      writer.EndObject();

      writer.EndObject();
      create_record(stringBuffer);

      DRETURN(true);
   }

   /** @brief Append one finished JSON object to the buffer
    * @param stringBuffer the rendered object
    */
   void
   JsonReportingFileWriter::create_record(rapidjson::StringBuffer &stringBuffer) {
      stringBuffer.Put('\n');

      sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
      buffer += stringBuffer.GetString();
      sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
   }

   bool
   JsonReportingFileWriter::create_new_job_record(lList **answer_list, const lListElem *job) {
      DENTER(TOP_LAYER);

      bool ret = true;

      if (job != nullptr) {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt64());
         write_json(writer, "type", "new_job");

         write_json(writer, "submission_time", ocs::tom::get<JB_submission_time>(job));
         write_json(writer, "job_number", ocs::tom::get<JB_job_number>(job));
         // A new_job record describes the whole job, before any ja_task/pe_task
         // exists. The dbwriter's sge_job primary key is
         // (j_job_number, j_task_number, j_pe_taskid); emit the "whole job"
         // sentinels (task_number = 0 -> normalized to -1 by JobManager, and
         // pe_taskid = "NONE") so the row created here matches the later
         // accounting record's primary key.
         write_json(writer, "task_number", 0);
         write_json(writer, "pe_taskid", NONE_STR);
         write_json(writer, "job_name", ocs::tom::get<JB_job_name>(job));
         write_json(writer, "owner", ocs::tom::get<JB_owner>(job));
         write_json(writer, "group", ocs::tom::get<JB_group>(job));
         write_json(writer, "project", ocs::tom::get<JB_project>(job));
         write_json(writer, "department", ocs::tom::get<JB_department>(job));
         write_json(writer, "account", ocs::tom::get<JB_account>(job));
         write_json(writer, "priority", ocs::tom::get<JB_priority>(job));

         writer.EndObject();
         create_record(stringBuffer);
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_job_log(lList **answer_list, uint64_t event_time, job_log_t type, const char *user,
                                           const char *host,
                                           const lListElem *job_report, const lListElem *job, const lListElem *ja_task,
                                           const lListElem *pe_task, const char *message) {
      DENTER(TOP_LAYER);

      bool ret = true;

      if (do_joblog && job != nullptr) {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         uint32_t job_id = ocs::tom::get<JB_job_number>(job);

         /* set ja_task_id:
          * -1, if we don't have a ja_task
          *  0, if we have a non array job
          *  task_number for array jobs
          */
         int ja_task_id = -1;
         if (ja_task != nullptr) {
            if (job_is_array(job)) {
               ja_task_id = (int) ocs::tom::get<JAT_task_number>(ja_task);
            } else {
               ja_task_id = 0;
            }
         }
         const char *pe_task_id = nullptr;
         if (pe_task != nullptr) {
            pe_task_id = ocs::tom::get_or_empty<PET_id>(pe_task);
         }

         uint32_t jstate;
         if (pe_task != nullptr) {
            jstate = ocs::tom::get<PET_status>(pe_task);
         } else if (ja_task != nullptr) {
            jstate = ocs::tom::get<JAT_status>(ja_task);
         } else {
            jstate = 0;
         }

         char state[20];
         *state = '\0';
         job_get_state_string(state, jstate);
         if (message == nullptr) {
            message = "";
         }

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt64());
         write_json(writer, "type", "job_log");

         write_json(writer, "event_time", event_time);
         write_json(writer, "event", get_job_log_name(type));
         write_json(writer, "job_number", job_id);
         write_json(writer, "task_number", ja_task_id);
         // "NONE" is the sentinel the classic reporting writer emits when
         // there is no pe_task; keep the JSON record on the same convention
         // so the dbwriter's primary key matches across record types.
         write_json(writer, "pe_taskid", pe_task_id != nullptr ? pe_task_id : NONE_STR);
         write_json(writer, "state", jstate);
         write_json(writer, "user", user);
         write_json(writer, "host", host);
         // write_json(writer, "state_time", state_time); man page: reserved field for later
         write_json(writer, "submission_time", ocs::tom::get<JB_submission_time>(job));
         write_json(writer, "job_name", ocs::tom::get<JB_job_name>(job));
         write_json(writer, "owner", ocs::tom::get<JB_owner>(job));
         write_json(writer, "group", ocs::tom::get<JB_group>(job));
         write_json(writer, "project", ocs::tom::get<JB_project>(job));
         write_json(writer, "department", ocs::tom::get<JB_department>(job));
         write_json(writer, "account", ocs::tom::get<JB_account>(job));
         write_json(writer, "priority", ocs::tom::get<JB_priority>(job));
         write_json(writer, "message", message);

         writer.EndObject();
         create_record(stringBuffer);
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_host_record(lList **answer_list, const lListElem *host, uint64_t report_time) {
      DENTER(TOP_LAYER);

      bool ret = true;

      if (host != nullptr) {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt64());
         write_json(writer, "type", "host");

         write_json(writer, "hostname", static_cast<const char *>(ocs::tom::get<EH_name>(host)));
         write_json(writer, "report_time", report_time);
         write_json(writer, "state", "X");

         if (write_load_values(writer, ocs::tom::get<EH_load_list>(host),
                           ocs::tom::get<EH_merged_report_variables>(host))) {

            // create the record only when we really wrote any load values
            writer.EndObject();
            create_record(stringBuffer);
         }
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_host_consumable_record(lList **answer_list, const lListElem *host,
                                                          const lListElem *job,
                                                          uint64_t report_time) {
      DENTER(TOP_LAYER);

      bool ret = true;

      if (host != nullptr) {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt64());
         write_json(writer, "type", "host_consumable");

         write_json(writer, "hostname", static_cast<const char *>(ocs::tom::get<EH_name>(host)));
         write_json(writer, "report_time", report_time);
         write_json(writer, "state", "X");

         if (write_consumables(writer,
                           ocs::tom::get<EH_resource_utilization>(host),
                           ocs::tom::get<EH_consumable_config_list>(host),
                           host, job)) {
            // create the record only when we really wrote any consumables
            writer.EndObject();
            create_record(stringBuffer);
         }
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_queue_record(lList **answer_list, const lListElem *queue, uint64_t report_time) {
      DENTER(TOP_LAYER);

      bool ret = true;

      if (queue == nullptr) {
         ret = false;
      } else {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt64());
         write_json(writer, "type", "queue");

         write_json(writer, "qname", ocs::tom::get<QU_qname>(queue));
         write_json(writer, "hostname", static_cast<const char *>(ocs::tom::get<QU_qhostname>(queue)));
         write_json(writer, "report_time", report_time);

         DSTRING_STATIC(dstr, 10);
         if (qinstance_state_append_to_dstring(queue, &dstr)) {
            write_json(writer, "state", sge_dstring_get_string(&dstr));
         }

         writer.EndObject();
         create_record(stringBuffer);
      }

      DRETURN(ret);

   }

   bool
   JsonReportingFileWriter::create_queue_consumable_record(lList **answer_list, const lListElem *host,
                                                           const lListElem *queue,
                                                           const lListElem *job, uint64_t report_time) {
      DENTER(TOP_LAYER);

      bool ret = true;

      if (host == nullptr || queue == nullptr) {
         ret = false;
      } else {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt64());
         write_json(writer, "type", "queue_consumable");

         write_json(writer, "qname", ocs::tom::get<QU_qname>(queue));
         write_json(writer, "hostname", static_cast<const char *>(ocs::tom::get<QU_qhostname>(queue)));
         write_json(writer, "report_time", report_time);

         DSTRING_STATIC(dstr, 10);
         if (qinstance_state_append_to_dstring(queue, &dstr)) {
            write_json(writer, "state", sge_dstring_get_string(&dstr));
         }

         if (write_consumables(writer,
                           ocs::tom::get<QU_resource_utilization>(queue),
                           ocs::tom::get<QU_consumable_config_list>(queue),
                           host, job)) {
            // create the record only when we really wrote any consumables
            writer.EndObject();
            create_record(stringBuffer);
         }
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_new_ar_record(lList **answer_list, const lListElem *ar, uint64_t report_time) {
      DENTER(TOP_LAYER);

      bool ret = true;

      if (ar == nullptr) {
         ret = false;
      } else {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt64());
         write_json(writer, "type", "new_ar");

         write_json(writer, "ar_submission_time", ocs::tom::get<AR_submission_time>(ar));
         write_json(writer, "ar_number", ocs::tom::get<AR_id>(ar));
         write_json(writer, "ar_owner", ocs::tom::get<AR_owner>(ar));

         writer.EndObject();
         create_record(stringBuffer);
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_ar_attribute_record(lList **answer_list, const lListElem *ar, uint64_t report_time) {
      DENTER(TOP_LAYER);

      bool ret = true;

      if (ar == nullptr) {
         ret = false;
      } else {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt64());
         write_json(writer, "type", "ar_attribute");

         write_json(writer, "ar_submission_time", ocs::tom::get<AR_submission_time>(ar));
         write_json(writer, "ar_number", ocs::tom::get<AR_id>(ar));
         write_json(writer, "ar_name", ocs::tom::get<AR_name>(ar));
         write_json(writer, "ar_account", ocs::tom::get<AR_account>(ar));
         // the key is left out for an AR which did not request a project
         write_json(writer, "ar_project", ocs::tom::get<AR_project>(ar));
         write_json(writer, "ar_start_time", ocs::tom::get<AR_start_time>(ar));
         write_json(writer, "ar_end_time", ocs::tom::get<AR_end_time>(ar));
         write_json(writer, "ar_granted_pe", ocs::tom::get<AR_pe>(ar));
         // the key is left out for an AR which did not request an allocation rule of its own
         write_json(writer, "ar_allocation_rule", ocs::tom::get<AR_allocation_rule>(ar));

         writer.Key("ar_granted_resources");
         writer.StartObject();
         for (ocs::tom::ConstElement<ocs::tom::CE> ep : ocs::tom::get<AR_resource_list>(ar)) {
            write_json(writer, ocs::tom::get<CE_name>(ep), ocs::tom::get<CE_stringval>(ep));
         }
         writer.EndObject();

         writer.EndObject();
         create_record(stringBuffer);
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_ar_log_record(lList **answer_list, const lListElem *ar, ar_state_event_t event,
                                                 const char *ar_description, uint64_t report_time) {
      DENTER(TOP_LAYER);

      bool ret = true;

      if (ar == nullptr) {
         ret = false;
      } else {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt64());
         write_json(writer, "type", "ar_log");

         write_json(writer, "ar_state_change_time", report_time);
         write_json(writer, "ar_submission_time", ocs::tom::get<AR_submission_time>(ar));
         write_json(writer, "ar_number", ocs::tom::get<AR_id>(ar));

         DSTRING_STATIC(dstr_state, 10);
         ar_state2dstring((ar_state_t) ocs::tom::get<AR_state>(ar), &dstr_state);
         write_json(writer, "ar_state", sge_dstring_get_string(&dstr_state));

         write_json(writer, "ar_event", ar_get_string_from_event(event));
         write_json(writer, "ar_message", ar_description);

         writer.EndObject();
         create_record(stringBuffer);
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_ar_acct_record(lList **answer_list, const lListElem *ar, uint64_t report_time) {
      DENTER(TOP_LAYER);
      bool ret = true;

      if (ar == nullptr) {
         ret = false;
      } else {
         for (ocs::tom::ConstElement<ocs::tom::JG> ep : ocs::tom::get<AR_granted_slots>(ar)) {
            const char *qinstance_name = ocs::tom::get<JG_qname>(ep);
            uint32_t slots = ocs::tom::get<JG_slots>(ep);
            DSTRING_STATIC(dstr_cqueue, MAX_STRING_SIZE);
            DSTRING_STATIC(dstr_host, MAX_STRING_SIZE);

            if (!cqueue_name_split(qinstance_name, &dstr_cqueue, &dstr_host, nullptr, nullptr)) {
               ret = false;
               continue;
            }

            create_single_ar_acct_record(ar, sge_dstring_get_string(&dstr_cqueue),
                                         sge_dstring_get_string(&dstr_host), slots, report_time);
         }
      }

      DRETURN(ret);
   }

   /** @brief Write one accounting record for one queue instance of a reservation
    *
    * A reservation spans queue instances, and the accounting is per
    * instance, so create_ar_acct_record() calls this once for each.
    *
    * @param ar the advance reservation (`AR_Type`)
    * @param cqueue_name the cluster queue
    * @param hostname the host
    * @param slots the slots reserved there
    * @param report_time the time to stamp the record with
    */
   void
   ocs::JsonReportingFileWriter::create_single_ar_acct_record(const lListElem *ar, const char *cqueue_name,
                                const char *hostname, uint32_t slots, uint64_t report_time) {

      rapidjson::StringBuffer stringBuffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

      writer.StartObject();
      write_json(writer, "time", sge_get_gmt64());
      write_json(writer, "type", "ar_acct");

      write_json(writer, "ar_termination_time", report_time);
      write_json(writer, "ar_submission_time", ocs::tom::get<AR_submission_time>(ar));
      write_json(writer, "ar_number", ocs::tom::get<AR_id>(ar));
      write_json(writer, "ar_qname", cqueue_name);
      write_json(writer, "ar_hostname", hostname);
      write_json(writer, "ar_slots", slots);

      writer.EndObject();
      create_record(stringBuffer);
   }

   void
   JsonReportingFileWriter::create_sharelog_record(monitoring_t *monitor) {
      DENTER(TOP_LAYER);

      if (sharelog_interval > 0) {
         // only create sharelog entries if we have a sharetree
         MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_READ), monitor);
         const lList *master_stree_list = *ocs::DataStore::get_master_list(SGE_TYPE_SHARETREE);
         if (lGetNumberOfElem(master_stree_list) == 0) {
            SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
         } else {
            rapidjson::StringBuffer stringBuffer;

            const lList *master_user_list = *ocs::DataStore::get_master_list(SGE_TYPE_USER);
            const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
            const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);

            /* define output format */
            format_t format;
            format.name_format = true;
            format.delim = nullptr;
            format.line_delim = "\n";
            format.rec_delim = "";
            format.str_format = "%s";
            format.field_names = nullptr;
            format.format_times = false;
            format.line_prefix = "sharelog";

            /* dump the sharetree data */
            sge_sharetree_print(nullptr, &stringBuffer, master_stree_list, master_user_list, master_project_list,
                                master_userset_list,
                                true, false, nullptr, &format);

            SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
            create_record(stringBuffer);
         }
      }

      DRETURN_VOID;
   }

   /** @brief Write a host's load values as a JSON object
    * @param writer the JSON writer
    * @param load_list the load values
    * @param variables which of them to write; all of them when nullptr
    * @return true on success
    */
   bool
   JsonReportingFileWriter::write_load_values(rapidjson::Writer<rapidjson::StringBuffer> &writer,
                                              const lList *load_list, const lList *variables) {
      DENTER(TOP_LAYER);

      bool ret = false; // did we write any values?

      // write load values only when requested
      if (lGetNumberOfElem(variables) > 0) {
         writer.Key("load_values");
         writer.StartObject();

         for_each_ep_lv (variable, variables) {
            const char *name;

            name = ocs::tom::get<STU_name>(variable);
            ocs::tom::ConstElement<ocs::tom::HL> load = ocs::tom::find_first<HL_name>(load_list, name);
            if (load != nullptr) {
               write_json(writer, name, ocs::tom::get<HL_value>(load));
            }
         }
         writer.EndObject();
         ret = true;
      }

      DRETURN(ret);
   }

   /** @brief Write the consumables of a host or queue instance as a JSON object
    * @param writer the JSON writer
    * @param actual what is currently in use
    * @param total what is configured
    * @param host the host, for the resource definitions
    * @param job the job the usage is attributed to, or nullptr for the host's own total
    * @return true on success
    */
   bool
   JsonReportingFileWriter::write_consumables(rapidjson::Writer<rapidjson::StringBuffer> &writer,
                                              const lList *actual, const lList *total,
                                              const lListElem *host, const lListElem *job) const {
      DENTER(TOP_LAYER);

      bool consumables_initialized = false;

      ocs::tom::ConstList<ocs::tom::STU> report_variables = ocs::tom::get<EH_merged_report_variables>(host);
      for_each_ep_lv(cep, actual) {
         const char *name = ocs::tom::get<RUE_name>(cep);
         bool log_variable = true;

         /*
          * if log_consumables is false, lookup if the consumable shall be logged
          * due to reporting_variables in global/local host
          */
         if (!log_consumables) {
            if (static_cast<const lListElem *>(ocs::tom::find_first<STU_name>(report_variables, name)) == nullptr) {
               log_variable = false;
            } else {
               /*
                * if we log consumables for a specific job, make sure to log only
                * consumables, which are requested by the job
                * slots is an implicit request - always log it if requested
                */
               if (strcmp(name, "slots") != 0 && job != nullptr) {
                  if (job_get_request(job, name) == nullptr) {
                     log_variable = false;
                  }
               }
            }
         }

         /* now do the logging, if requested */
         if (log_variable) {
            // create the consumables array only when we actually log at least one value
            if (!consumables_initialized) {
               writer.Key("consumables");
               writer.StartObject();
               consumables_initialized = true;
            }
            const lListElem *tep = ocs::tom::find_first<CE_name>(total, name);
            if (tep != nullptr) {
               writer.Key(name);
               writer.StartObject();

               DSTRING_STATIC(dstr, 512);
               utilization_print_to_dstring(cep, &dstr);
               write_json(writer, "utilization", sge_dstring_get_string(&dstr));

               sge_dstring_clear(&dstr);
               centry_print_resource_to_dstring(tep, &dstr);
               write_json(writer, "capacity", sge_dstring_get_string(&dstr));

               writer.EndObject();
            }
         }
      }

      // if we did actually write at least one consumable, close the array
      if (consumables_initialized) {
         writer.EndObject();
      }

      DRETURN(consumables_initialized);
   }
}
