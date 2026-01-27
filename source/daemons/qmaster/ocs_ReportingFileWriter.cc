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

#include <filesystem>
#include <fstream>

#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_usage.h"

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "ocs_ReportingFileWriter.h"
#include "ocs_JsonAccountingFileWriter.h"
#include "ocs_JsonReportingFileWriter.h"
#include "ocs_MonitoringFileWriter.h"
#include "sge_reporting_qmaster.h"
#include "sge_rusage.h"

#include "msg_qmaster.h"
#include "msg_common.h"
#include "sge_string.h"
#include "sge_spool.h"

namespace ocs {
   // static attributes
   std::array<ReportingFileWriter *, ReportingFileWriter::NUM_WRITERS> ReportingFileWriter::writers;

   // configuration parameters
   std::string ReportingFileWriter::reporting_params;
   std::string ReportingFileWriter::usage_patterns;
   std::vector<std::pair<std::string, std::string>> ReportingFileWriter::usage_pattern_list;

   // We have two mutexes to protec access to the writers and the configuration parameters.
   // Make sure to use the mutexes in the right order, i.e., always lock the writer_mutex before
   // the config_mutex; otherwise we might end up in a deadlock situation.
   // This order is required as in the writing functions we loop over all writers (need to lock them),
   // and in the writers doing their work we might need to access the configuration parameters.

   // protecting the writers array
   pthread_mutex_t ReportingFileWriter::writer_mutex = PTHREAD_MUTEX_INITIALIZER;
   std::string ReportingFileWriter::writer_mutex_name = "reporting_writer_mutex";
   // protecting the configuration parameters
   pthread_mutex_t ReportingFileWriter::config_mutex = PTHREAD_MUTEX_INITIALIZER;
   std::string ReportingFileWriter::config_mutex_name = "reporting_config_mutex";

   /**
    * initialize all configured writers and make them read their configuration from the
    * reporting params (@see ReportingFileWriter::update_config())
    */
   void ReportingFileWriter::initialize() {
      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      // create the Writers
      if (mconf_get_old_accounting()) {
         writers[CLASSIC_ACCOUNTING] = new ClassicAccountingFileWriter;
      } else {
         writers[JSON_ACCOUNTING] = new JsonAccountingFileWriter;
      }
      if (mconf_get_do_reporting()) {
         if (mconf_get_old_reporting()) {
            writers[CLASSIC_REPORTING] = new ClassicReportingFileWriter;
         } else {
            writers[JSON_REPORTING] = new JsonReportingFileWriter;
         }
      }

      if (mconf_get_do_monitoring()) {
         writers[JSON_MONITORING] = new MonitoringFileWriter;
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      // make Writers read their configuration
      update_config_all();
   }

   /**
    * shutdown all writers
    */
   void ReportingFileWriter::shutdown() {
      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (int i = 0; i < NUM_WRITERS; i++) {
         if (writers[i] != nullptr) {
            delete writers[i];
            writers[i] = nullptr;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
   }

   /**
    * triggers flushing data to file for all configured writers
    * writers decide if they actually need to flush data
    *
    * @return true on success, else false
    */
   bool ReportingFileWriter::flush_all() {
      bool ret = true;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr && !w->flush()) {
            ret = false;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return ret;
   }

   /**
    * triggers actions of all configured writers, e.g.
    * - writing of the sharelog
    * - flushing of data
    *
    * @param monitor monitor is used for recording the time required for acquiring a lock
    * (for writing the sharelog)
    * @return timestamp of the next trigger operation
    */
   u_long64 ReportingFileWriter::trigger_all(monitoring_t *monitor) {
      u_long64 next_trigger = U_LONG64_MAX;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr) {
            u_long64 next = w->trigger(monitor);
            if (next < next_trigger) {
               next_trigger = next;
            }
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return next_trigger;
   }

   /**
    * If the reporting_params were changed
    * - creates newly enabled writers
    * - removes the no longer necessary writers
    * - makes all configured writers read their configuration from the reporting_params
    */
   void ReportingFileWriter::update_config_all() {
      bool config_changed = false;
      const char *rp_str = mconf_get_reporting_params();
      std::string current_reporting_params = rp_str ? rp_str : "";
      sge_free(&rp_str);

      // check if the config has changed and update it
      sge_mutex_lock(config_mutex_name.c_str(), __func__, __LINE__, &config_mutex);
      if (current_reporting_params != reporting_params) {
         config_changed = true;
         reporting_params = current_reporting_params;
      }
      sge_mutex_unlock(config_mutex_name.c_str(), __func__, __LINE__, &config_mutex);

      if (config_changed) {
         // if we switched accounting type, update the writers
         sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
         if (mconf_get_old_accounting()) {
            // we might have switched accounting type to classic
            if (writers[CLASSIC_ACCOUNTING] == nullptr) {
               writers[CLASSIC_ACCOUNTING] = new ClassicAccountingFileWriter;
            }
            if (writers[JSON_ACCOUNTING] != nullptr) {
               delete writers[JSON_ACCOUNTING];
               writers[JSON_ACCOUNTING] = nullptr;
            }
         } else {
            // we might have switched accounting type to json
            if (writers[JSON_ACCOUNTING] == nullptr) {
               writers[JSON_ACCOUNTING] = new JsonAccountingFileWriter;
            }
            if (writers[CLASSIC_ACCOUNTING] != nullptr) {
               delete writers[CLASSIC_ACCOUNTING];
               writers[CLASSIC_ACCOUNTING] = nullptr;
            }
         }

         if (mconf_get_do_reporting()) {
            if (mconf_get_old_reporting()) {
               if (writers[CLASSIC_REPORTING] == nullptr) {
                  // reporting has been enabled
                  writers[CLASSIC_REPORTING] = new ClassicReportingFileWriter;
               }
               if (writers[JSON_REPORTING] != nullptr) {
                  // reporting has been disabled
                  delete writers[JSON_REPORTING];
                  writers[JSON_REPORTING] = nullptr;
               }
            } else {
               if (writers[JSON_REPORTING] == nullptr) {
                  // reporting has been enabled
                  writers[JSON_REPORTING] = new JsonReportingFileWriter;
               }
               if (writers[CLASSIC_REPORTING] != nullptr) {
                  // reporting has been disabled
                  delete writers[CLASSIC_REPORTING];
                  writers[CLASSIC_REPORTING] = nullptr;
               }
            }
         } else {
            if (writers[CLASSIC_REPORTING] != nullptr) {
               // reporting has been disabled
               delete writers[CLASSIC_REPORTING];
               writers[CLASSIC_REPORTING] = nullptr;
            }
            if (writers[JSON_REPORTING] != nullptr) {
               // reporting has been disabled
               delete writers[JSON_REPORTING];
               writers[JSON_REPORTING] = nullptr;
            }
         }

         if (mconf_get_do_monitoring()) {
            if (writers[JSON_MONITORING] == nullptr) {
               writers[JSON_MONITORING] = new MonitoringFileWriter;
            }
         } else {
            if (writers[JSON_MONITORING] != nullptr) {
               delete writers[JSON_MONITORING];
               writers[JSON_MONITORING] = nullptr;
            }
         }
         sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

         // usage patterns for accounting
         std::string new_usage_patterns = mconf_get_usage_patterns();
         sge_mutex_lock(config_mutex_name.c_str(), __func__, __LINE__, &config_mutex);
         if (new_usage_patterns != usage_patterns) {
            usage_patterns = new_usage_patterns;
            usage_pattern_list.clear();
            struct saved_vars_s *context = nullptr;
            const char *pattern = sge_strtok_r(usage_patterns.c_str(), ";", &context);
            while (pattern != nullptr) {
               struct saved_vars_s *inner_context = nullptr;
               const char *name = sge_strtok_r(pattern, ":", &inner_context);
               if (name != nullptr) {
                  const char *value = sge_strtok_r(nullptr, ":", &inner_context);
                  if (value != nullptr) {
                     usage_pattern_list.push_back({name, value});
                  }
               }
               sge_free_saved_vars(inner_context);

               // next pattern
               pattern = sge_strtok_r(nullptr, ";", &context);
            }
            sge_free_saved_vars(context);
         }
         sge_mutex_unlock(config_mutex_name.c_str(), __func__, __LINE__, &config_mutex);

         // now configure all active Writers
         for (auto w: writers) {
            if (w != nullptr) {
               w->update_config();
            }
         }

         // delete the existing timer and set up a new immediate one
         // which will update the next trigger time
         reporting_reinitialize_timed_event();
      }
   }

   // the following static methods are wrappers calling the object methods for every writer
   bool ReportingFileWriter::create_new_job_records(lList **answer_list, const lListElem *job) {
      bool ret = true;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr && !w->create_new_job_record(answer_list, job)) {
            ret = false;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return ret;
   }

   bool ReportingFileWriter::create_job_logs(lList **answer_list, u_long64 event_time, const job_log_t type,
                                             const char *user, const char *host, const lListElem *job_report,
                                             const lListElem *job, const lListElem *ja_task, const lListElem *pe_task,
                                             const char *message) {
      bool ret = true;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr &&
             !w->create_job_log(answer_list, event_time, type, user, host, job_report, job, ja_task, pe_task,
                                message)) {
            ret = false;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return ret;
   }

   bool ReportingFileWriter::create_acct_records(lList **answer_list, lListElem *job_report, lListElem *job,
                                                 lListElem *ja_task, bool intermediate) {
      bool ret = true;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr && !w->create_acct_record(answer_list, job_report, job, ja_task, intermediate)) {
            ret = false;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return ret;
   }

   bool ReportingFileWriter::create_host_records(lList **answer_list, const lListElem *host, u_long64 report_time) {
      bool ret = true;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr && !w->create_host_record(answer_list, host, report_time)) {
            ret = false;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return ret;
   }

   bool
   ReportingFileWriter::create_host_consumable_records(lList **answer_list, const lListElem *host, const lListElem *job,
                                                       u_long64 report_time) {
      bool ret = true;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr && !w->create_host_consumable_record(answer_list, host, job, report_time)) {
            ret = false;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return ret;
   }

   bool ReportingFileWriter::create_queue_records(lList **answer_list, const lListElem *queue, u_long64 report_time) {
      bool ret = true;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr && !w->create_queue_record(answer_list, queue, report_time)) {
            ret = false;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return ret;
   }

   bool ReportingFileWriter::create_queue_consumable_records(lList **answer_list, const lListElem *host,
                                                             const lListElem *queue, const lListElem *job,
                                                             u_long64 report_time) {
      bool ret = true;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr && !w->create_queue_consumable_record(answer_list, host, queue, job, report_time)) {
            ret = false;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return ret;
   }

   bool ReportingFileWriter::create_new_ar_records(lList **answer_list, const lListElem *ar, u_long64 report_time) {
      bool ret = true;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr && !w->create_new_ar_record(answer_list, ar, report_time)) {
            ret = false;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return ret;
   }

   bool
   ReportingFileWriter::create_ar_attribute_records(lList **answer_list, const lListElem *ar, u_long64 report_time) {
      bool ret = true;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr && !w->create_ar_attribute_record(answer_list, ar, report_time)) {
            ret = false;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return ret;
   }

   bool ReportingFileWriter::create_ar_log_records(lList **answer_list, const lListElem *ar, ar_state_event_t state,
                                                   const char *ar_description, u_long64 report_time) {
      bool ret = true;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr && !w->create_ar_log_record(answer_list, ar, state, ar_description, report_time)) {
            ret = false;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return ret;
   }

   bool ReportingFileWriter::create_ar_acct_records(lList **answer_list, const lListElem *ar, u_long64 report_time) {
      bool ret = true;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr && !w->create_ar_acct_record(answer_list, ar, report_time)) {
            ret = false;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return ret;
   }

   bool ReportingFileWriter::create_monitoring_records(const char *json_data) {
      bool ret = true;

      sge_mutex_lock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);
      for (auto w: writers) {
         if (w != nullptr && !w->create_monitoring_record(json_data)) {
            ret = false;
         }
      }
      sge_mutex_unlock(writer_mutex_name.c_str(), __func__, __LINE__, &writer_mutex);

      return ret;
   }

   // object methods

   /**
   * Checks if it is necessary to write an intermediate accounting record
   * for the given ja_task or pe_task.
   *
   * An intermediate accounting record is written
   *    - reporting is activated at all.
   *    - when the first usage record for a job is received after midnight.
   *    - the job hasn't just started some seconds before midnight.
   *      This is an optimization to limit the number of intermediate
   *      accounting records in throughput clusters with short job runtimes.
   *      The minimum runtime required for an intermediate record to be written
   *      is defined in INTERMEDIATE_MIN_RUNTIME.
   *
   * A further optimization has been done: To reduce the overhead caused by
   * this function (called with every job report), the check will only be done
   * in a time window starting at midnight. The length of the time window is
   * defined in INTERMEDIATE_ACCT_WINDOW.
   *
   * @note For debugging this feature set the define TEST_INTERMEDIATE_HOURLY - see below.
   *       This will write intermediate accounting records not only daily,
   *       but every hour.
   *
   * @param job
   * @param ja_task
   * @param pe_task
   * @return true, if writing of an intermediate accounting record is required, else false
   */
#if 0
#define TEST_INTERMEDIATE_HOURLY
#endif
   bool ReportingFileWriter::is_intermediate_acct_required(const lListElem *job, const lListElem *ja_task,
                                                           const lListElem *pe_task) {
      DENTER(TOP_LAYER);

      // Valid input data?
      if (job == nullptr || ja_task == nullptr) {
         /* @todo I18N */
         WARNING("reporting_is_intermediate_acct_required: invalid input data\n");
#if defined (ENABLE_DEBUG_CHECKS)
         abort();
#endif
         DRETURN(false);
      }

      // If reporting isn't active, we needn't write intermediate usage.
      if (!mconf_get_do_reporting()) {
         DRETURN(false);
      }

      time_t last_intermediate, now, start_time;
      struct tm tm_last_intermediate{}, tm_now{};

      // Optimization: Only do the following actions "shortly after midnight".
      now = time(nullptr);
      localtime_r(&now, &tm_now);
      if (
#if not defined(TEST_INTERMEDIATE_HOURLY) // For development and debugging: Write intermediate data every hour.
         tm_now.tm_hour != 0 ||
#endif
         tm_now.tm_min > INTERMEDIATE_ACCT_WINDOW) {
         DRETURN(false);
      }

       // Optimization: Do not write intermediate usage for jobs that just
       // "started a short time before".
      if (pe_task != nullptr) {
         start_time = sge_gmt64_to_time_t(lGetUlong64(pe_task, PET_start_time));
      } else {
         start_time = sge_gmt64_to_time_t(lGetUlong64(ja_task, JAT_start_time));
      }

      if ((now - start_time) < (INTERMEDIATE_MIN_RUNTIME + tm_now.tm_min * 60 + tm_now.tm_sec)) {
         DRETURN(false);
      }

       // Try to read time of an earlier intermediate report.
       // If no intermediate report has been written so far, use the job's / pe task's start time.
      if (pe_task != nullptr) {
         last_intermediate = sge_gmt64_to_time_t(usage_list_get_ulong64_usage(lGetList(pe_task, PET_reported_usage),
                                                                 LAST_INTERMEDIATE, 0));
      } else {
         last_intermediate = sge_gmt64_to_time_t(usage_list_get_ulong64_usage(lGetList(ja_task, JAT_reported_usage_list),
                                                                 LAST_INTERMEDIATE, 0));
      }
      if (last_intermediate == 0) {
         last_intermediate = start_time;
      }

      // Compare the day portion of last_intermediate vs. current time.
      // If the day (in hourly debug mode: the hour) changed, we have to write an intermediate report.
      bool ret = false;
      localtime_r(&last_intermediate, &tm_last_intermediate);
      if (
#if defined(TEST_INTERMEDIATE_HOURLY) /* for development and debugging: write intermediate data every hour */
          tm_last_intermediate.tm_hour < tm_now.tm_hour ||
#endif
          tm_last_intermediate.tm_yday < tm_now.tm_yday || tm_last_intermediate.tm_year < tm_now.tm_year) {
         char timebuffer[100];
         DSTRING_STATIC(buffer_dstring, 100);
         INFO(MSG_REPORTING_INTERMEDIATE_SS,
              job_get_key(lGetUlong(job, JB_job_number), lGetUlong(ja_task, JAT_task_number),
                          pe_task != nullptr ? lGetString(pe_task, PET_id) : nullptr, &buffer_dstring),
              asctime_r(&tm_now, timebuffer));
         ret = true;
      }

      DRETURN(ret);
   }

   /**
    * If there is data to write then flush it to file.
    * If the file does not yet exist then a comment containing the OGE version
    * is written to the head of the file.
    * Possible file IO errors are logged to the messages file.
    *
    * @return true on success, false on error
    */
   bool ReportingFileWriter::flush() {
      bool ret = true;

      DENTER(TOP_LAYER);

      sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);

      /* do we have anything to write? */
      size_t size = buffer.length();
      if (size > 0) {
         bool write_comment = false;

         if (!std::filesystem::exists(filename) || std::filesystem::is_empty(filename)) {
            if (write_comment_header) {
               write_comment = true;
            }
         }

         std::ofstream stream;
         stream.open(filename, std::ios::app);
         if (!stream.is_open()) {
            DSTRING_STATIC(error_dstring, MAX_STRING_SIZE);
            ERROR(MSG_ERROROPENINGFILEFORWRITING_SS, filename.c_str(), sge_strerror(errno, &error_dstring));
            ret = false;
         }

         /* write comment if necessary */
         if (ret) {
            if (write_comment) {
               int spool_ret;
               DSTRING_STATIC(version_dstring, MAX_STRING_SIZE);
               const char *version_string;

               version_string = feature_get_product_name(FS_VERSION, &version_dstring);
               spool_ret = sge_spoolmsg_write(stream, COMMENT_CHAR, version_string);
               if (spool_ret != 0) {
                  DSTRING_STATIC(error_dstring, MAX_STRING_SIZE);
                  ERROR(MSG_ERRORWRITINGFILE_SS, filename.c_str(), sge_strerror(errno, &error_dstring));
                  ret = false;
               }
            }
         }

         /* write data */
         if (ret) {
            stream << buffer;
            if (stream.fail()) {
               DSTRING_STATIC(error_dstring, MAX_STRING_SIZE);
               ERROR(MSG_ERRORWRITINGFILE_SS, filename.c_str(), sge_strerror(errno, &error_dstring));
               ret = false;
            }
         }

         /* close file */
         stream.close();
         if (stream.fail()) {
            DSTRING_STATIC(error_dstring, MAX_STRING_SIZE);
            ERROR(MSG_ERRORCLOSINGFILE_SS, filename.c_str(), sge_strerror(errno, &error_dstring));
            ret = false;
         }

         /* clear the buffer. We do this regardless of the result of
          * the writing command. Otherwise, if writing the report file failed
          * over a longer time period, the reporting buffer could grow endlessly.
          */
         buffer.clear();
      }

      sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);

      DRETURN(ret);
   }

   /**
    * Trigger function of the ReportingFileWriter base class.
    * It flushes the cached accounting/reporting data to file
    * in the configured interval (reporting_params accounting_flush_time or flush_time).
    *
    * @param monitor not used here but trigger functions of derived classes need it
    * @return the timestamp of the next flush time
    */
   u_long64 ReportingFileWriter::trigger(monitoring_t *monitor) {
      // trigger for flushing the data are the same for all accounting/reporting classes
      u_long64 now = sge_get_gmt64();
      if (next_flush_time <= now) {
         flush();
         next_flush_time = now + config_flush_time;
      }

      return next_flush_time;
   }

   void ReportingFileWriter::update_config() {
      // if the flush_time changed, need to re-calculate the next_flush_time
      auto new_config_flush_time = sge_gmt32_to_gmt64(mconf_get_reporting_flush_time());
      update_config_flush_time(new_config_flush_time);
   }

   void ReportingFileWriter::update_config_flush_time(u_long64 new_flush_time) {
      if (new_flush_time != config_flush_time) {
         if (next_flush_time != 0) {
            next_flush_time = next_flush_time - config_flush_time + new_flush_time;
         }
         config_flush_time = new_flush_time;
      }
   }
} // namespace ocs
