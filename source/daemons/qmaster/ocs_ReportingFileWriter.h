#pragma once
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
 * @brief The writers behind the accounting, reporting and monitoring files
 *
 * qmaster keeps up to five of these open at once - the classic accounting and
 * reporting files, their JSON equivalents, and the monitoring file - and every
 * event that is worth recording is offered to all of them. Each decides for
 * itself whether it wants that record and in which format.
 *
 * That is why every event has a pair of functions: a static
 * `create_*_records()` the rest of qmaster calls, which fans out to the
 * writers, and a virtual `create_*_record()` each writer implements. A writer
 * that does not care about an event inherits the base's no-op.
 *
 * Records are buffered and flushed on a timer rather than written as they
 * happen, because a busy cluster produces them faster than a disk will take
 * them.
 */

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tom/ocs_Types.h"

#include "sgeobj/sge_advance_reservation.h"

#include "uti/sge_bootstrap_files.h"

#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JR.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_QU.h"

#include "sge_job_qmaster.h"
#include "sgeobj/sge_answer.h"

namespace ocs {
   /** @brief Base of the accounting, reporting and monitoring file writers
    *
    * @see ocs_ReportingFileWriter.h for why every event has both a static and
    *      a virtual function.
    */
   class ReportingFileWriter {
      /** @brief The writers qmaster keeps open, and the size of that array */
      enum {
         CLASSIC_ACCOUNTING = 0,   ///< The delimiter-separated accounting file `qacct` reads
         CLASSIC_REPORTING,        ///< The delimiter-separated reporting file
         JSON_ACCOUNTING,          ///< The same accounting records as JSON
         JSON_REPORTING,           ///< The same reporting records as JSON
         JSON_MONITORING,          ///< qmaster's own monitoring output
         NUM_WRITERS               ///< How many there are; not a writer
      };

   private:
      bool write_comment_header;   ///< Whether this file starts with a comment describing its columns
      // @todo: do we need to use a mutex for accessing all these config values?
      static std::array<ReportingFileWriter *,NUM_WRITERS> writers;   ///< The writers, indexed by the enum above
      static pthread_mutex_t writer_mutex;      ///< Guards #writers
      static std::string writer_mutex_name;     ///< Its name, for the lock monitoring
      static std::string reporting_params;      ///< The `reporting_params` configuration, as last read

   protected:
      std::string filename;            ///< The file this writer appends to
      std::string buffer;              ///< Records written but not yet flushed
      pthread_mutex_t buffer_mutex;    ///< Guards #buffer
      uint64_t config_flush_time;      ///< How often the configuration says to flush
      uint64_t next_flush_time;        ///< When this writer next flushes
      static std::vector<std::pair<std::string, std::string>> usage_pattern_list;   ///< Which usage values to record, parsed
      static std::vector<std::string> online_usage_vars;   ///< Which usage values to record while a job still runs
      static bool sync_write;          ///< Whether a record is written straight through rather than buffered
      static pthread_mutex_t config_mutex;   ///< Guards the configuration above
      static std::string config_mutex_name;  ///< Its name, for the lock monitoring

   public:
      /** @brief Build a writer for one file
       * @param filename the file to append to
       * @param do_write_comment_header whether to start it with a column header
       */
      explicit ReportingFileWriter(std::string filename, bool do_write_comment_header)
              : write_comment_header(do_write_comment_header), filename(std::move(filename)),
              buffer_mutex(PTHREAD_MUTEX_INITIALIZER), config_flush_time(0), next_flush_time(0) {
      };

      /** @brief Flush whatever is still buffered before the writer goes away */
      virtual ~ReportingFileWriter() {
         flush();
      }

      // Class methods
      static void initialize();

      static void shutdown();

      static bool flush_all();

      static uint64_t trigger_all(monitoring_t *monitor);

      static void update_config_all();

      static bool
      create_new_job_records(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::JB> job);

      static bool
      create_job_logs(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, uint64_t event_time, job_log_t, const char *user,
                      const char *host,
                      ocs::tom::ConstElement<ocs::tom::JR> job_report, ocs::tom::ConstElement<ocs::tom::JB> job,
                      ocs::tom::ConstElement<ocs::tom::JAT> ja_task, ocs::tom::ConstElement<ocs::tom::PET> pe_task,
                      const char *message);

      static bool
      create_acct_records(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::Element<ocs::tom::JR> job_report,
                          ocs::tom::Element<ocs::tom::JB> job, ocs::tom::Element<ocs::tom::JAT> ja_task,
                          bool intermediate);

      static bool
      create_online_usage_records(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                  ocs::tom::ConstElement<ocs::tom::JR> job_report,
                                  ocs::tom::ConstElement<ocs::tom::JB> job,
                                  ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                  ocs::tom::ConstElement<ocs::tom::PET> pe_task, bool aggregate_pe_tasks);

      static bool
      is_online_usage_required();

      static bool
      create_host_records(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::EH> host,
                          uint64_t report_time);

      static bool
      create_host_consumable_records(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                     ocs::tom::ConstElement<ocs::tom::EH> host,
                                     ocs::tom::ConstElement<ocs::tom::JB> job, uint64_t report_time);

      static bool
      create_queue_records(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::QU> queue,
                           uint64_t report_time);

      static bool
      create_queue_consumable_records(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                      ocs::tom::ConstElement<ocs::tom::EH> host,
                                      ocs::tom::ConstElement<ocs::tom::QU> queue,
                                      ocs::tom::ConstElement<ocs::tom::JB> job, uint64_t report_time);

      static bool
      create_new_ar_records(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::AR> ar,
                            uint64_t report_time);

      static bool
      create_ar_attribute_records(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::AR> ar,
                                  uint64_t report_time);

      static bool
      create_ar_log_records(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::AR> ar,
                            ar_state_event_t state, const char *ar_description, uint64_t report_time);

      static bool
      create_ar_acct_records(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::AR> ar,
                             uint64_t report_time);

      static bool
      is_intermediate_acct_required(ocs::tom::ConstElement<ocs::tom::JB> job,
                                    ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                    ocs::tom::ConstElement<ocs::tom::PET> pe_task);

      static bool
      create_monitoring_records(const char *json_data);


      // Object methods
      virtual bool flush();

      virtual uint64_t trigger(monitoring_t *monitor);

      virtual void update_config();

      void update_config_flush_time(uint64_t new_flush_time);

      /** @brief Record a job that has just been submitted
       * @param answer_list receives error messages
       * @param job the job (`JB`)
       * @return true on success
       */
      virtual bool
      create_new_job_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                            ocs::tom::ConstElement<ocs::tom::JB> job) { return true; }

      /** @brief Record a change in a job's life: submitted, started, deleted, finished
       * @param answer_list receives error messages
       * @param event_time when it happened
       * @param user who caused it
       * @param host the host it happened on
       * @param job_report the job report the execution host sent
       * @param job the job (`JB`)
       * @param ja_task the array task
       * @param pe_task the PE task
       * @param message a description of what happened
       * @return true on success
       */
      virtual bool
      create_job_log(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, uint64_t event_time, const job_log_t,
                     const char *user, const char *host,
                     ocs::tom::ConstElement<ocs::tom::JR> job_report, ocs::tom::ConstElement<ocs::tom::JB> job,
                     ocs::tom::ConstElement<ocs::tom::JAT> ja_task, ocs::tom::ConstElement<ocs::tom::PET> pe_task,
                     const char *message) { return true; }

      /** @brief Record a finished job, or an intermediate record for one still running
       * @param answer_list receives error messages
       * @param job_report the job report the execution host sent
       * @param job the job (`JB`)
       * @param ja_task the array task
       * @param intermediate whether this is an interim record for a job that is still running
       * @return true on success
       */
      virtual bool
      create_acct_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::Element<ocs::tom::JR> job_report,
                         ocs::tom::Element<ocs::tom::JB> job, ocs::tom::Element<ocs::tom::JAT> ja_task,
                         bool intermediate) = 0;

      /** @brief Record the usage of a job that is still running
       * @param answer_list receives error messages
       * @param job_report the job report the execution host sent
       * @param job the job (`JB`)
       * @param ja_task the array task
       * @param pe_task the PE task
       * @param aggregate_pe_tasks whether the PE tasks are summed rather than reported individually
       * @return true on success
       */
      virtual bool
      create_online_usage_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                 ocs::tom::ConstElement<ocs::tom::JR> job_report,
                                 ocs::tom::ConstElement<ocs::tom::JB> job,
                                 ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                 ocs::tom::ConstElement<ocs::tom::PET> pe_task, bool aggregate_pe_tasks) {
         return true;
      }

      /** @brief Record an execution host and its load values
       * @param answer_list receives error messages
       * @param host the host it happened on
       * @param report_time the time to stamp the record with
       * @return true on success
       */
      virtual bool
      create_host_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::EH> host,
                         uint64_t report_time) {
         return true;
      }

      /** @brief Record the consumables a job took from a host
       * @param answer_list receives error messages
       * @param host the host it happened on
       * @param job the job (`JB`)
       * @param report_time the time to stamp the record with
       * @return true on success
       */
      virtual bool
      create_host_consumable_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                    ocs::tom::ConstElement<ocs::tom::EH> host,
                                    ocs::tom::ConstElement<ocs::tom::JB> job, uint64_t report_time) { return true; }

      /** @brief Record a queue instance and its state
       * @param answer_list receives error messages
       * @param queue the queue instance (`QU`)
       * @param report_time the time to stamp the record with
       * @return true on success
       */
      virtual bool
      create_queue_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::QU> queue,
                          uint64_t report_time) {
         return true;
      }

      /** @brief Record the consumables a job took from a queue instance
       * @param answer_list receives error messages
       * @param host the host it happened on
       * @param queue the queue instance (`QU`)
       * @param job the job (`JB`)
       * @param report_time the time to stamp the record with
       * @return true on success
       */
      virtual bool
      create_queue_consumable_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                     ocs::tom::ConstElement<ocs::tom::EH> host,
                                     ocs::tom::ConstElement<ocs::tom::QU> queue,
                                     ocs::tom::ConstElement<ocs::tom::JB> job, uint64_t report_time) { return true; }

      /** @brief Record an advance reservation that has just been created
       * @param answer_list receives error messages
       * @param ar the advance reservation (`AR`)
       * @param report_time the time to stamp the record with
       * @return true on success
       */
      virtual bool
      create_new_ar_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::AR> ar,
                           uint64_t report_time) {
         return true;
      }

      /** @brief Record the attributes of an advance reservation
       * @param answer_list receives error messages
       * @param ar the advance reservation (`AR`)
       * @param report_time the time to stamp the record with
       * @return true on success
       */
      virtual bool
      create_ar_attribute_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::AR> ar,
                                 uint64_t report_time) {
         return true;
      }

      /** @brief Record a change in an advance reservation's state
       * @param answer_list receives error messages
       * @param ar the advance reservation (`AR`)
       * @param state the state it moved to
       * @param ar_description a description of the change
       * @param report_time the time to stamp the record with
       * @return true on success
       */
      virtual bool
      create_ar_log_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::AR> ar,
                           ar_state_event_t state, const char *ar_description, uint64_t report_time) { return true; }

      /** @brief Record the accounting record of an advance reservation
       * @param answer_list receives error messages
       * @param ar the advance reservation (`AR`)
       * @param report_time the time to stamp the record with
       * @return true on success
       */
      virtual bool
      create_ar_acct_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::AR> ar,
                            uint64_t report_time) {
         return true;
      }

      /** @brief Record a monitoring snapshot
       * @param json_data the snapshot, already rendered as JSON
       * @return true on success
       */
      virtual bool
      create_monitoring_record(const char *json_data) { return true; }
   };
}
