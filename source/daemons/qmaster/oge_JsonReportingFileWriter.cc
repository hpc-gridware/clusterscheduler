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

#include "category.h"

#include "sched/sge_resource_utilization.h"
#include "sched/sge_sharetree_printing.h"

#include "sgeobj/oge_DataStore.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_str.h"

#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sge_rusage.h"

#include "oge_JsonReportingFileWriter.h"

namespace oge {
   bool JsonReportingFileWriter::create_acct_record(lList **answer_list, lListElem *job_report,
                                                    lListElem *job, lListElem *ja_task, bool intermediate) {
      bool ret = true;

      DENTER(TOP_LAYER);

      const lList *master_userset_list = *oge::DataStore::get_master_list(SGE_TYPE_USERSET);
      const lList *master_project_list = *oge::DataStore::get_master_list(SGE_TYPE_PROJECT);
      const lList *master_rqs_list = *oge::DataStore::get_master_list(SGE_TYPE_RQS);
      DSTRING_STATIC(category_dstring, MAX_STRING_SIZE);

      // get category string
      sge_build_job_category_dstring(&category_dstring, job, master_userset_list, master_project_list, nullptr,
                                     master_rqs_list);
      const char *category_string = sge_dstring_get_string(&category_dstring);

      // get accounting data
      rapidjson::StringBuffer json_buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(json_buffer);
      ret = sge_write_rusage(nullptr, &writer, job_report, job, ja_task, category_string, 0,
                             false, true);
      if (ret) {
         // append data to buffer
         json_buffer.Put('\n');
         sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &mutex);
         buffer += json_buffer.GetString();
         sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &mutex);
      }

      DRETURN(ret);
   }

   static void
   write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, int value) {
      writer.Key(key);
      writer.Int(value);
   }

   static void
   write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, u_long32 value) {
      writer.Key(key);
      writer.Uint64(value);
   }

#if 0
   static void
   write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, double value) {
      writer.Key(key);
      writer.Double(value);
   }
#endif

   static void
   write_json(rapidjson::Writer<rapidjson::StringBuffer> &writer, const char *key, const char *value) {
      if (value != nullptr) {
         writer.Key(key);
         writer.String(value);
      }
   }

   void
   JsonReportingFileWriter::create_record(rapidjson::StringBuffer &stringBuffer) {
      stringBuffer.Put('\n');

      sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &mutex);
      buffer += stringBuffer.GetString();
      sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &mutex);
   }

   bool
   JsonReportingFileWriter::create_new_job_record(lList **answer_list, const lListElem *job) {
      bool ret = true;

      DENTER(TOP_LAYER);

      if (job != nullptr) {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt());
         write_json(writer, "type", "new_job");

         write_json(writer, "submission_time", lGetUlong(job, JB_submission_time));
         write_json(writer, "job_number", lGetUlong(job, JB_job_number));
         // according to man page the following two fields should be there, but are not / cannot
         //write_json(writer, "task_number", );
         //write_json(writer, "pe_taskid", );
         write_json(writer, "job_name", lGetString(job, JB_job_name));
         write_json(writer, "owner", lGetString(job, JB_owner));
         write_json(writer, "group", lGetString(job, JB_group));
         write_json(writer, "project", lGetString(job, JB_project));
         write_json(writer, "department", lGetString(job, JB_department));
         write_json(writer, "account", lGetString(job, JB_account));
         write_json(writer, "priority", lGetUlong(job, JB_priority));

         writer.EndObject();
         create_record(stringBuffer);
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_job_log(lList **answer_list, u_long32 event_time, job_log_t type, const char *user,
                                           const char *host,
                                           const lListElem *job_report, const lListElem *job, const lListElem *ja_task,
                                           const lListElem *pe_task, const char *message) {
      bool ret = true;

      DENTER(TOP_LAYER);

      if (do_joblog && job != nullptr) {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         u_long32 job_id = lGetUlong(job, JB_job_number);

         /* set ja_task_id:
          * -1, if we don't have a ja_task
          *  0, if we have a non array job
          *  task_number for array jobs
          */
         int ja_task_id = -1;
         if (ja_task != nullptr) {
            if (job_is_array(job)) {
               ja_task_id = (int) lGetUlong(ja_task, JAT_task_number);
            } else {
               ja_task_id = 0;
            }
         }
         const char *pe_task_id = nullptr;
         if (pe_task != nullptr) {
            pe_task_id = lGetStringNotNull(pe_task, PET_id);
         }

         u_long32 jstate;
         if (pe_task != nullptr) {
            jstate = lGetUlong(pe_task, PET_status);
         } else if (ja_task != nullptr) {
            jstate = lGetUlong(ja_task, JAT_status);
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
         write_json(writer, "time", sge_get_gmt());
         write_json(writer, "type", "job_log");

         write_json(writer, "event_time", event_time);
         write_json(writer, "event", get_job_log_name(type));
         write_json(writer, "job_number", job_id);
         write_json(writer, "task_number", ja_task_id);
         write_json(writer, "pe_taskid", pe_task_id);
         write_json(writer, "state", jstate);
         write_json(writer, "user", user);
         write_json(writer, "host", host);
         // write_json(writer, "state_time", state_time); man page: reserved field for later
         write_json(writer, "submission_time", lGetUlong(job, JB_submission_time));
         write_json(writer, "job_name", lGetString(job, JB_job_name));
         write_json(writer, "owner", lGetString(job, JB_owner));
         write_json(writer, "group", lGetString(job, JB_group));
         write_json(writer, "project", lGetString(job, JB_project));
         write_json(writer, "department", lGetString(job, JB_department));
         write_json(writer, "account", lGetString(job, JB_account));
         write_json(writer, "priority", lGetUlong(job, JB_priority));
         write_json(writer, "message", message);

         writer.EndObject();
         create_record(stringBuffer);
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_host_record(lList **answer_list, const lListElem *host, u_long32 report_time) {
      bool ret = true;

      DENTER(TOP_LAYER);

      if (host != nullptr) {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt());
         write_json(writer, "type", "host");

         write_json(writer, "hostname", lGetHost(host, EH_name));
         write_json(writer, "report_time", report_time);
         write_json(writer, "state", "X");

         if (write_load_values(writer, lGetList(host, EH_load_list),
                           lGetList(host, EH_merged_report_variables))) {

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
                                                          u_long32 report_time) {

      bool ret = true;

      DENTER(TOP_LAYER);

      if (host != nullptr) {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt());
         write_json(writer, "type", "host_consumable");

         write_json(writer, "hostname", lGetHost(host, EH_name));
         write_json(writer, "report_time", report_time);
         write_json(writer, "state", "X");

         if (write_consumables(writer,
                           lGetList(host, EH_resource_utilization),
                           lGetList(host, EH_consumable_config_list),
                           host, job)) {
            // create the record only when we really wrote any consumables
            writer.EndObject();
            create_record(stringBuffer);
         }
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_queue_record(lList **answer_list, const lListElem *queue, u_long32 report_time) {
      bool ret = true;

      DENTER(TOP_LAYER);

      if (queue == nullptr) {
         ret = false;
      } else {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt());
         write_json(writer, "type", "queue");

         write_json(writer, "qname", lGetString(queue, QU_qname));
         write_json(writer, "hostname", lGetHost(queue, QU_qhostname));
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
                                                           const lListElem *job, u_long32 report_time) {
      bool ret = true;

      DENTER(TOP_LAYER);

      if (host == nullptr || queue == nullptr) {
         ret = false;
      } else {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt());
         write_json(writer, "type", "queue_consumable");

         write_json(writer, "qname", lGetString(queue, QU_qname));
         write_json(writer, "hostname", lGetHost(queue, QU_qhostname));
         write_json(writer, "report_time", report_time);

         DSTRING_STATIC(dstr, 10);
         if (qinstance_state_append_to_dstring(queue, &dstr)) {
            write_json(writer, "state", sge_dstring_get_string(&dstr));
         }

         if (write_consumables(writer,
                           lGetList(queue, QU_resource_utilization),
                           lGetList(queue, QU_consumable_config_list),
                           host, job)) {
            // create the record only when we really wrote any consumables
            writer.EndObject();
            create_record(stringBuffer);
         }
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_new_ar_record(lList **answer_list, const lListElem *ar, u_long32 report_time) {
      bool ret = true;

      DENTER(TOP_LAYER);

      if (ar == nullptr) {
         ret = false;
      } else {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt());
         write_json(writer, "type", "new_ar");

         write_json(writer, "ar_submission_time", lGetUlong(ar, AR_submission_time));
         write_json(writer, "ar_number", lGetUlong(ar, AR_id));
         write_json(writer, "ar_owner", lGetString(ar, AR_owner));

         writer.EndObject();
         create_record(stringBuffer);
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_ar_attribute_record(lList **answer_list, const lListElem *ar, u_long32 report_time) {
      bool ret = true;

      DENTER(TOP_LAYER);

      if (ar == nullptr) {
         ret = false;
      } else {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt());
         write_json(writer, "type", "ar_attribute");

         write_json(writer, "ar_submission_time", lGetUlong(ar, AR_submission_time));
         write_json(writer, "ar_number", lGetUlong(ar, AR_id));
         write_json(writer, "ar_name", lGetString(ar, AR_name));
         write_json(writer, "ar_account", lGetString(ar, AR_account));
         write_json(writer, "ar_start_time", lGetUlong(ar, AR_start_time));
         write_json(writer, "ar_end_time", lGetUlong(ar, AR_end_time));
         write_json(writer, "ar_granted_pe", lGetString(ar, AR_pe));

         writer.Key("ar_granted_resources");
         writer.StartObject();
         const lListElem *ep;
         for_each_ep(ep, lGetList(ar, AR_resource_list)) {
            write_json(writer, lGetString(ep, CE_name), lGetString(ep, CE_stringval));
         }
         writer.EndObject();

         writer.EndObject();
         create_record(stringBuffer);
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_ar_log_record(lList **answer_list, const lListElem *ar, ar_state_event_t event,
                                                 const char *ar_description, u_long32 report_time) {
      bool ret = true;

      DENTER(TOP_LAYER);

      if (ar == nullptr) {
         ret = false;
      } else {
         rapidjson::StringBuffer stringBuffer;
         rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

         writer.StartObject();
         write_json(writer, "time", sge_get_gmt());
         write_json(writer, "type", "ar_log");

         write_json(writer, "ar_state_change_time", report_time);
         write_json(writer, "ar_submission_time", lGetUlong(ar, AR_submission_time));
         write_json(writer, "ar_number", lGetUlong(ar, AR_id));

         DSTRING_STATIC(dstr_state, 10);
         ar_state2dstring((ar_state_t) lGetUlong(ar, AR_state), &dstr_state);
         write_json(writer, "ar_state", sge_dstring_get_string(&dstr_state));

         write_json(writer, "ar_event", ar_get_string_from_event(event));
         write_json(writer, "ar_message", ar_description);

         writer.EndObject();
         create_record(stringBuffer);
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::create_ar_acct_record(lList **answer_list, const lListElem *ar, u_long32 report_time) {
      bool ret = true;

      DENTER(TOP_LAYER);

      if (ar == nullptr) {
         ret = false;
      } else {
         const lListElem *ep;
         for_each_ep(ep, lGetList(ar, AR_granted_slots)) {
            const char *qinstance_name = lGetString(ep, JG_qname);
            u_long32 slots = lGetUlong(ep, JG_slots);
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

   void
   oge::JsonReportingFileWriter::create_single_ar_acct_record(const lListElem *ar, const char *cqueue_name,
                                const char *hostname, u_long32 slots, u_long32 report_time) {

      rapidjson::StringBuffer stringBuffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(stringBuffer);

      writer.StartObject();
      write_json(writer, "time", sge_get_gmt());
      write_json(writer, "type", "ar_acct");

      write_json(writer, "ar_termination_time", report_time);
      write_json(writer, "ar_submission_time", lGetUlong(ar, AR_submission_time));
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
         const lList *master_stree_list = *oge::DataStore::get_master_list(SGE_TYPE_SHARETREE);
         if (lGetNumberOfElem(master_stree_list) > 0) {
            rapidjson::StringBuffer stringBuffer;

            const lList *master_user_list = *oge::DataStore::get_master_list(SGE_TYPE_USER);
            const lList *master_userset_list = *oge::DataStore::get_master_list(SGE_TYPE_USERSET);
            const lList *master_project_list = *oge::DataStore::get_master_list(SGE_TYPE_PROJECT);

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
            MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_READ), monitor);

            sge_sharetree_print(nullptr, &stringBuffer, master_stree_list, master_user_list, master_project_list,
                                master_userset_list,
                                true, false, nullptr, &format);

            SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
            create_record(stringBuffer);
         }
      }

      DRETURN_VOID;
   }

   bool
   JsonReportingFileWriter::write_load_values(rapidjson::Writer<rapidjson::StringBuffer> &writer,
                                              const lList *load_list, const lList *variables) {
      DENTER(TOP_LAYER);

      bool ret = false; // did we write any values?

      // write load values only when requested
      if (lGetNumberOfElem(variables) > 0) {
         writer.Key("load_values");
         writer.StartObject();

         const lListElem *variable;
         for_each_ep (variable, variables) {
            const char *name;
            const lListElem *load;

            name = lGetString(variable, STU_name);
            load = lGetElemStr(load_list, HL_name, name);
            if (load != nullptr) {
               write_json(writer, name, lGetString(load, HL_value));
            }
         }
         writer.EndObject();
         ret = true;
      }

      DRETURN(ret);
   }

   bool
   JsonReportingFileWriter::write_consumables(rapidjson::Writer<rapidjson::StringBuffer> &writer,
                                              const lList *actual, const lList *total,
                                              const lListElem *host, const lListElem *job) const {

      DENTER(TOP_LAYER);

      bool consumables_initialized = false;

      const lList *report_variables = lGetList(host, EH_merged_report_variables);
      const lListElem *cep;
      for_each_ep(cep, actual) {
         const char *name = lGetString(cep, RUE_name);
         bool log_variable = true;

         /*
          * if log_consumables == false, lookup if the consumable shall be logged
          * due to reporting_variables in global/local host
          */
         if (!log_consumables) {
            if (lGetElemStr(report_variables, STU_name, name) == nullptr) {
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
            const lListElem *tep = lGetElemStr(total, CE_name, name);
            if (tep != nullptr) {
               writer.Key(name);
               writer.StartObject();

               DSTRING_STATIC(dstr, 100);
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
