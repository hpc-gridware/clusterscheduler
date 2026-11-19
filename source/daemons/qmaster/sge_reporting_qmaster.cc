/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The classic delimiter-separated accounting and reporting files
 */

#include <ctime>

#include "uti/sge_dstring.h"
#include "uti/sge_lock.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_Category.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_str.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_STU.h"
#include "sgeobj/tom/ocs_tom_UA.h"
#include "sgeobj/tom/ocs_tom_CT.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"

#include "sched/sge_resource_utilization.h"
#include "sched/sge_sharetree_printing.h"

#include "ocs_ReportingFileWriter.h"
#include "sge_job_qmaster.h"
#include "sge_reporting_qmaster.h"
#include "sge_rusage.h"

/****** qmaster/reporting/--Introduction ***************************
*  NAME
*     qmaster reporting -- creation of a reporting file 
*
*  FUNCTION
*     This module provides a set of functions that are used to write
*     the SGE reporting file and the accounting file.
*
*     See the manpages accounting.5 and qacct.1 for information about the
*     accounting file.
*
*     The reporting file contains entries for queues, hosts, accounting
*     and sharetree usage.
*     It can for example be used to transfer SGE data useful for reporting
*     and analysis purposes to a database.
*
*  SEE ALSO
*     qmaster/reporting/reporting_initialize()
*     qmaster/reporting/reporting_shutdown()
*     qmaster/reporting/reporting_trigger_handler()
*******************************************************************************/

/* ------------- public functions ------------- */
/**
 * @brief Initialize the reporting module
 *
 * Register the event handler for the reporting trigger and create the
 * ReportingFileWriters. Creating the writers also creates the reporting
 * trigger itself.
 *
 * @note MT-NOTE: reporting_initialize() is MT safe.
 *
 * @see #reporting_shutdown, `te_add()`
 */
void
reporting_initialize() {
   DENTER(TOP_LAYER);

   // Register the handler before creating the writers. ReportingFileWriter::initialize()
   // ends in update_config_all(), which calls reporting_reinitialize_timed_event() and so
   // already adds the reporting trigger - and that event is due immediately. Were the
   // handler not registered by then, the timer thread could deliver the event to nobody
   // and the trigger would be lost for the lifetime of the qmaster.
   te_register_event_handler(reporting_trigger_handler, TYPE_REPORTING_TRIGGER);

   // Create the ReportingFileWriters. This also creates the reporting trigger, which we
   // always need for flushing reporting files and checking for new reporting configuration.
   ocs::ReportingFileWriter::initialize();

   DRETURN_VOID;
}

/** @brief Make the reporting trigger fire again immediately
 *
 * Called when the reporting configuration changed: the pending trigger was
 * scheduled with the old interval, so it is dropped and a new one is queued for
 * right now. The next handler run then re-reads the configuration and schedules
 * itself with the new interval.
 */
void
reporting_reinitialize_timed_event() {
   DENTER(TOP_LAYER);

   te_delete_all_one_time_events(TYPE_REPORTING_TRIGGER);
   te_event_t ev = te_new_event(sge_get_gmt64(), TYPE_REPORTING_TRIGGER, ONE_TIME_EVENT, 1, 0, nullptr);
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
}

/**
 * @brief Shutdown the reporting module
 *
 * @param answer_list used to return error messages
 * @param do_spool if set to true changes must be spooled if set to false don't spool changes
 *
 * @return true on success, false on error.
 *
 * @note MT-NOTE: reporting_shutdown() is MT safe
 *
 * @see #reporting_initialize
 */
bool
reporting_shutdown(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bool do_spool) {
   DENTER(TOP_LAYER);

   bool ret = true;
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   if (do_spool) {
      /* flush the last reporting values, suppress adding new timer */
      if (!ocs::ReportingFileWriter::flush_all()) {
         answer_list_output(&alp);
         ret = false;
      }
   }

   ocs::ReportingFileWriter::shutdown();

   DRETURN(ret);
}

/**
 * @brief Process timed event
 *
 * @param anEvent timed event
 * @param monitor for monitoring qmaster threads
 *
 * @note MT-NOTE: reporting_trigger_handler() is MT safe.
 *
 * @see `te_deliver()`, `te_add()`
 */
void
reporting_trigger_handler(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   uint64_t next_flush = ocs::ReportingFileWriter::trigger_all(monitor);
   te_event_t ev = te_new_event(next_flush, te_get_type(anEvent), ONE_TIME_EVENT, 1, 0, nullptr);
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
} /* reporting_trigger_handler() */

/** @brief Has an interim accounting record already been written for this task?
 *
 * The reported usage list of the array task, or of the PE task the job report
 * is about, is created when the first intermediate accounting record is
 * written, so its existence answers the question.
 *
 * @param job_report the job report (`JR_Type`); its PE task id selects the PE task
 * @param ja_task the array task (`JAT_Type`)
 * @return true if an intermediate accounting record has been written before
 *
 * @note MT-NOTE: intermediate_usage_written() is MT-safe
 */
bool
intermediate_usage_written(const ocs::tom::ConstElement<ocs::tom::JR> job_report,
                           const ocs::tom::ConstElement<ocs::tom::JAT> ja_task) {
   DENTER(TOP_LAYER);

   bool ret = false;
   const char *pe_task_id;
   ocs::tom::ConstList<ocs::tom::UA> reported_usage = nullptr;

   /* do we have a tightly integrated parallel task? */
   pe_task_id = ocs::tom::get<JR_pe_task_id_str>(job_report);
   if (pe_task_id != nullptr) {
      const auto pe_task = ocs::tom::get<JAT_task_list>(ja_task).find_first<PET_id>(pe_task_id);
      if (pe_task) {
         reported_usage = ocs::tom::get<PET_reported_usage>(pe_task);
      }
   } else {
      reported_usage = ocs::tom::get<JAT_reported_usage_list>(ja_task);
   }

   /* the reported usage list will be created when the first intermediate
    * acct record is written
    */
   if (reported_usage) {
      ret = true;
   }

   DRETURN(ret);
}

// object methods

/**
 * @brief Write the accounting record of a finished job to the classic accounting file
 *
 * Intermediate records are not written to the accounting file, only the
 * record at job end.
 *
 * @param answer_list used to report error messages
 * @param job_report the job report the execution host sent (`JR_Type`)
 * @param job the job (`JB_Type`)
 * @param ja_task the array task that finished (`JAT_Type`)
 * @param intermediate whether this is an interim record for a job that is still running
 * @return true on success, false on error
 */
bool
ocs::ClassicAccountingFileWriter::create_acct_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                     const ocs::tom::Element<ocs::tom::JR> job_report,
                                                     const ocs::tom::Element<ocs::tom::JB> job,
                                                     const ocs::tom::Element<ocs::tom::JAT> ja_task,
                                                     bool intermediate) {
   DENTER(TOP_LAYER);
   bool ret = true;

   /* accounting records will only be written at job end, not for intermediate
    * reports
    */
   if (!intermediate) {
      // get the job category string
      const auto master_category_list = ocs::DataStore::master_list<SGE_TYPE_CATEGORY>();
      const uint32_t category_id = ocs::tom::get<JB_category_id>(job);
      const auto category = master_category_list.find_first<CT_id>(category_id);
      const char *category_string = ocs::tom::get<CT_str>(category);

      dstring job_dstring = DSTRING_INIT;
      ret = sge_write_rusage(&job_dstring, nullptr, job_report, job, ja_task,
                             category_string, nullptr, REPORTING_DELIMITER, false, false);
      if (ret) {
         /* write accounting file */
         sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
         buffer += sge_dstring_get_string(&job_dstring);
         sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
      }

      // If immediate flushing is enabled, flush the buffer now
      if (accounting_immediate_flush) {
         ret = flush();
      }

      sge_dstring_free(&job_dstring);
   }

   DRETURN(ret);
}

/** @brief Append one line to the reporting buffer
 *
 * Writes `<timestamp>:<type>:<data>` under the buffer mutex. The timestamp is
 * taken here rather than by the caller, so that the order of the lines in the
 * file matches the order in which they were buffered.
 *
 * @param type the record type, the second field of a reporting file line
 * @param data the already formatted remainder of the line
 */
void
ocs::ClassicReportingFileWriter::create_record(const char *type, const char *data) {
   sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
   buffer += std::to_string(sge_gmt64_to_time_t(sge_get_gmt64()));
   buffer += REPORTING_DELIMITER;
   buffer += type;
   buffer += REPORTING_DELIMITER;
   buffer += data;
   sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
}

/**
 * @brief Create an accounting record
 *
 * Create an accounting record.
 * Depending on the cluster configuration, parameter reporting_params,
 * accounting is written to the accounting file and/or the reporting file.
 * During the runtime of jobs, intermediate accounting records can be written
 * to the reporting file. This is usually done at midnight, to have correct
 * daily accounting information for long running jobs.
 *
 * @param answer_list used to report error messages
 * @param job_report job report from execd
 * @param job job referenced in report
 * @param ja_task array task that finished
 * @param intermediate is this an intermediate accounting record?
 *
 * @return true on success, false on error
 *
 * @note MT-NOTE: create_acct_record() is not MT safe as the
 *       MT safety of called functions sge_build_job_category and
 *       sge_write_rusage is not defined.
 */
/* @todo: we should also pass pe_task. It is known in the code pieces where
 * create_acct_record is called and we needn't search it from ja_task
 */
bool
ocs::ClassicReportingFileWriter::create_acct_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                    const ocs::tom::Element<ocs::tom::JR> job_report,
                                                    const ocs::tom::Element<ocs::tom::JB> job,
                                                    const ocs::tom::Element<ocs::tom::JAT> ja_task, bool intermediate) {
   DENTER(TOP_LAYER);

   // get the job category string
   const auto master_category_list = ocs::DataStore::master_list<SGE_TYPE_CATEGORY>();
   const uint32_t category_id = ocs::tom::get<JB_category_id>(job);
   const auto category = master_category_list.find_first<CT_id>(category_id);
   const char *category_string = ocs::tom::get<CT_str>(category);

   // reporting records will be written both for intermediate and final job reports
   /* job_dstring might have been filled with accounting record - this one
    * contains total accounting values.
    * If we have written intermediate accounting records earlier, or this
    * call will write intermediate accounting, we have to create our own
    * accounting record.
    * Otherwise, (final accounting record, no intermediate acct done before),
    * we can reuse the accounting record.
    */
   bool intermediate_written = intermediate_usage_written(job_report, ja_task);
   bool do_intermediate = (intermediate_written || intermediate);

   dstring job_dstring = DSTRING_INIT;
   bool ret = sge_write_rusage(&job_dstring, nullptr, job_report, job, ja_task,
                               category_string, nullptr, REPORTING_DELIMITER,
                               do_intermediate, false);
   if (ret) {
      create_record("acct", sge_dstring_get_string(&job_dstring));
   }

   sge_dstring_free(&job_dstring);

   DRETURN(ret);
}

/**
 * @brief Write a new_job record for a job that has just been submitted
 *
 * Nothing is written unless job logging is enabled.
 *
 * @param answer_list used to report error messages
 * @param job the job (`JB_Type`), may be an empty handle
 * @return true on success
 */
bool
ocs::ClassicReportingFileWriter::create_new_job_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                       const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (do_joblog && job != nullptr) {
      dstring job_dstring = DSTRING_INIT;

      uint32_t job_number, priority;
      uint64_t submission_time;
      const char *job_name, *owner, *group, *project, *department, *account;

      job_number = ocs::tom::get<JB_job_number>(job);
      priority = ocs::tom::get<JB_priority>(job);
      submission_time = sge_gmt64_to_time_t(ocs::tom::get<JB_submission_time>(job));
      job_name = ocs::tom::get_or_empty<JB_job_name>(job);
      owner = ocs::tom::get_or_empty<JB_owner>(job);
      group = ocs::tom::get_or_empty<JB_group>(job);
      project = ocs::tom::get_or_empty<JB_project>(job);
      department = ocs::tom::get_or_empty<JB_department>(job);
      account = ocs::tom::get_or_empty<JB_account>(job);

      sge_dstring_sprintf(&job_dstring,
                          sge_u64"%c"
                          sge_u32"%c"
                          "%d%c"
                          "%s%c"
                          "%s%c"
                          "%s%c"
                          "%s%c"
                          "%s%c"
                          "%s%c"
                          "%s%c"
                          sge_u32
                          "\n",
                          submission_time, REPORTING_DELIMITER,
                          job_number, REPORTING_DELIMITER,
                          -1, REPORTING_DELIMITER, /* means: no ja_task yet */
                          NONE_STR, REPORTING_DELIMITER,
                          job_name, REPORTING_DELIMITER,
                          owner, REPORTING_DELIMITER,
                          group, REPORTING_DELIMITER,
                          project, REPORTING_DELIMITER,
                          department, REPORTING_DELIMITER,
                          account, REPORTING_DELIMITER,
                          priority);

      /* write record to reporting buffer */
      create_record("new_job", sge_dstring_get_string(&job_dstring));
      sge_dstring_free(&job_dstring);
   }

   DRETURN(ret);
}

/**
 * @brief Write a job_log record for a change in a job's life
 *
 * Nothing is written unless job logging is enabled.
 *
 * @param answer_list used to report error messages
 * @param event_time when it happened
 * @param type which change it was
 * @param user who caused it
 * @param host the host it happened on
 * @param job_report the job report the execution host sent (`JR_Type`), not used here
 * @param job the job (`JB_Type`), may be an empty handle
 * @param ja_task the array task (`JAT_Type`), may be an empty handle
 * @param pe_task the PE task (`PET_Type`), may be an empty handle
 * @param message a description of what happened, may be nullptr
 * @return true on success
 */
bool
ocs::ClassicReportingFileWriter::create_job_log(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, uint64_t event_time,
                                                const job_log_t type, const char *user, const char *host,
                                                const ocs::tom::ConstElement<ocs::tom::JR> job_report,
                                                const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                                const ocs::tom::ConstElement<ocs::tom::PET> pe_task,
                                                const char *message) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (do_joblog && job != nullptr) {
      dstring job_dstring = DSTRING_INIT;

      uint32_t job_id;
      int ja_task_id = -1;
      const char *pe_task_id = NONE_STR;
      uint32_t state_time = 0, jstate;
      const char *event;
      char state[20];
      uint32_t priority;
      uint64_t submission_time;
      const char *job_name, *owner, *group, *project, *department, *account;

      job_id = ocs::tom::get<JB_job_number>(job);

      /* set ja_task_id:
       * -1, if we don't have a ja_task
       *  0, if we have a non array job
       *  task_number for array jobs
       */
      if (ja_task != nullptr) {
         if (job_is_array(job)) {
            ja_task_id = (int) ocs::tom::get<JAT_task_number>(ja_task);
         } else {
            ja_task_id = 0;
         }
      }
      if (pe_task != nullptr) {
         pe_task_id = ocs::tom::get_or_empty<PET_id>(pe_task);
      }

      /* JG: TODO: implement the whole job/task state mess */
      if (pe_task != nullptr) {
         jstate = ocs::tom::get<PET_status>(pe_task);
      } else if (ja_task != nullptr) {
         jstate = ocs::tom::get<JAT_status>(ja_task);
      } else {
         jstate = 0;
      }

      event = get_job_log_name(type);

      *state = '\0';
      job_get_state_string(state, jstate);
      if (message == nullptr) {
         message = "";
      }

      priority = ocs::tom::get<JB_priority>(job);
      submission_time = sge_gmt64_to_time_t(ocs::tom::get<JB_submission_time>(job));
      job_name = ocs::tom::get_or_empty<JB_job_name>(job);
      owner = ocs::tom::get_or_empty<JB_owner>(job);
      group = ocs::tom::get_or_empty<JB_group>(job);
      project = ocs::tom::get_or_empty<JB_project>(job);
      department = ocs::tom::get_or_empty<JB_department>(job);
      account = ocs::tom::get_or_empty<JB_account>(job);

      sge_dstring_sprintf(&job_dstring,
                          sge_u64 "%c%s%c" sge_u32 "%c%d%c%s%c%s%c%s%c%s%c" sge_u32 "%c" sge_u32 "%c" sge_u64
                          "%c%s%c%s%c%s%c%s%c%s%c%s%c%s\n",
                          (uint64_t)sge_gmt64_to_time_t(event_time), REPORTING_DELIMITER,
                          event, REPORTING_DELIMITER,
                          job_id, REPORTING_DELIMITER,
                          ja_task_id, REPORTING_DELIMITER,
                          pe_task_id, REPORTING_DELIMITER,
                          state, REPORTING_DELIMITER,
                          user, REPORTING_DELIMITER,
                          host, REPORTING_DELIMITER,
                          state_time, REPORTING_DELIMITER,
                          priority, REPORTING_DELIMITER,
                          submission_time, REPORTING_DELIMITER,
                          job_name, REPORTING_DELIMITER,
                          owner, REPORTING_DELIMITER,
                          group, REPORTING_DELIMITER,
                          project, REPORTING_DELIMITER,
                          department, REPORTING_DELIMITER,
                          account, REPORTING_DELIMITER,
                          message);
      /* write record to reporting buffer */
      DPRINTF((sge_dstring_get_string(&job_dstring)));
      create_record("job_log", sge_dstring_get_string(&job_dstring));
      sge_dstring_free(&job_dstring);
   }

   DRETURN(ret);
}

/**
 * @brief Dump consumables to a buffer
 *
 * @param answer_list used to return error messages
 * @param buffer target buffer
 * @param actual actual consumable values
 * @param total configured consumable values
 * @param host host for which to output data
 * @param job optional: job which changes consumables, may be an empty handle
 *
 * @note MT-NOTE: reporting_write_consumables() is MT safe
 */
void
ocs::ClassicReportingFileWriter::reporting_write_consumables(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                             dstring *buffer,
                                                             const ocs::tom::ConstList<ocs::tom::RUE> actual,
                                                             const ocs::tom::ConstList<ocs::tom::CE> total,
                                                             const ocs::tom::ConstElement<ocs::tom::EH> host,
                                                             const ocs::tom::ConstElement<ocs::tom::JB> job) const {
   DENTER(TOP_LAYER);

   for (const auto cep : actual) {
      const char *name = ocs::tom::get<RUE_name>(cep);
      bool log_variable = true;

      /*
       * if log_consumables is false, lookup if the consumable shall be logged
       * due to reporting_variables in global/local host
       */
      if (!log_consumables) {
         const auto report_variables = ocs::tom::get<EH_merged_report_variables>(host);
         if (!report_variables.find_first<STU_name>(name)) {
            log_variable = false;
         } else {
            /*
             * if we log consumables for a specific job, make sure to log only
             * consumables, which are requested by the job
             * slots is an implicit request - always log it if requested
             */
            if (strcmp(name, SGE_ATTR_SLOTS) != 0 && job != nullptr) {
               if (!job_get_request(job, name)) {
                  log_variable = false;
               }
            }
         }
      }

      /* now do the logging, if requested */
      if (log_variable) {
         const auto tep = total.find_first<CE_name>(name);
         if (tep) {
            sge_dstring_append(buffer, name);
            sge_dstring_append_char(buffer, '=');
            utilization_print_to_dstring(cep, buffer);
            sge_dstring_append_char(buffer, '=');
            centry_print_resource_to_dstring(tep, buffer);

            if (cep.next()) {
               sge_dstring_append_char(buffer, ',');
            }
         }
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Create queue reporting record
 *
 * @param answer_list used to return error messages
 * @param queue the queue to output
 * @param report_time time of the last load report
 *
 * @return true on success, false on error
 *
 * @note MT-NOTE: create_queue_record() is MT safe
 */
bool
ocs::ClassicReportingFileWriter::create_queue_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                     const ocs::tom::ConstElement<ocs::tom::QU> queue,
                                                     uint64_t report_time) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (queue != nullptr) {
      dstring queue_dstring = DSTRING_INIT;

      sge_dstring_sprintf(&queue_dstring, "%s%c%s%c" sge_u64 "%c",
                          ocs::tom::get<QU_qname>(queue),
                          REPORTING_DELIMITER,
                          static_cast<const char *>(ocs::tom::get<QU_qhostname>(queue)),
                          REPORTING_DELIMITER,
                          (uint64_t)sge_gmt64_to_time_t(report_time),
                          REPORTING_DELIMITER);
      qinstance_state_append_to_dstring(queue, &queue_dstring);
      sge_dstring_append_char(&queue_dstring, '\n');

      /* write record to reporting buffer */
      create_record("queue", sge_dstring_get_string(&queue_dstring));

      sge_dstring_free(&queue_dstring);
   }

   DRETURN(ret);
}

/**
 * @brief Write queue consumables
 *
 * @param answer_list used to return error messages
 * @param host host on which the qinstance is located
 * @param queue queue instance to output
 * @param job optional: job which changes consumables
 * @param report_time time when consumables changed
 *
 * @return true on success, false on error
 *
 * @note MT-NOTE: create_queue_consumable_record() is MT safe
 */
bool
ocs::ClassicReportingFileWriter::create_queue_consumable_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                                const ocs::tom::ConstElement<ocs::tom::EH> host,
                                                                const ocs::tom::ConstElement<ocs::tom::QU> queue,
                                                                const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                                uint64_t report_time) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (host != nullptr && queue != nullptr) {
      dstring consumable_dstring = DSTRING_INIT;

      /* dump consumables */
      reporting_write_consumables(answer_list, &consumable_dstring,
                                  ocs::tom::get<QU_resource_utilization>(queue),
                                  ocs::tom::get<QU_consumable_config_list>(queue),
                                  host, job);

      if (sge_dstring_strlen(&consumable_dstring) > 0) {
         dstring queue_dstring = DSTRING_INIT;
         sge_dstring_sprintf(&queue_dstring, "%s%c%s%c" sge_u64 "%c",
                             ocs::tom::get<QU_qname>(queue),
                             REPORTING_DELIMITER,
                             static_cast<const char *>(ocs::tom::get<QU_qhostname>(queue)),
                             REPORTING_DELIMITER,
                             (uint64_t)sge_gmt64_to_time_t(report_time),
                             REPORTING_DELIMITER);
         qinstance_state_append_to_dstring(queue, &queue_dstring);
         sge_dstring_sprintf_append(&queue_dstring, "%c%s\n",
                                    REPORTING_DELIMITER,
                                    sge_dstring_get_string(&consumable_dstring));

         /* write record to reporting buffer */
         create_record("queue_consumable", sge_dstring_get_string(&queue_dstring));
         sge_dstring_free(&queue_dstring);
      }

      sge_dstring_free(&consumable_dstring);
   }


   DRETURN(ret);
}

/**
 * @brief Create host reporting record
 *
 * @param answer_list used to return error messages
 * @param host the host to output
 * @param report_time time of the last load report
 *
 * @return true on success, false on error
 *
 * @note MT-NOTE: create_host_record() is MT safe
 */
bool
ocs::ClassicReportingFileWriter::create_host_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                    const ocs::tom::ConstElement<ocs::tom::EH> host,
                                                    uint64_t report_time) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (host != nullptr) {
      dstring load_dstring = DSTRING_INIT;

      write_load_values(answer_list, &load_dstring, ocs::tom::get<EH_load_list>(host),
                        ocs::tom::get<EH_merged_report_variables>(host));

      /* As long as we have no host status information, dump host data only if we have
       * load values to report.
       */
      if (sge_dstring_strlen(&load_dstring) > 0) {
         dstring host_dstring = DSTRING_INIT;
         sge_dstring_sprintf(&host_dstring, "%s%c" sge_u64 "%c%s%c%s\n",
                             static_cast<const char *>(ocs::tom::get<EH_name>(host)), REPORTING_DELIMITER,
                             (uint64_t)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                             "X", REPORTING_DELIMITER,
                             sge_dstring_get_string(&load_dstring));
         /* write record to reporting buffer */
         create_record("host", sge_dstring_get_string(&host_dstring));

         sge_dstring_free(&host_dstring);
      }

      sge_dstring_free(&load_dstring);
   }

   DRETURN(ret);
}

/**
 * @brief Write host consumables
 *
 * @param answer_list used to return error messages
 * @param host host to output
 * @param job optional: job which changes consumables
 * @param report_time time when consumables changed
 *
 * @return true on success, false on error
 *
 * @note MT-NOTE: create_host_consumable_record() is MT safe
 */
bool
ocs::ClassicReportingFileWriter::create_host_consumable_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                               const ocs::tom::ConstElement<ocs::tom::EH> host,
                                                               const ocs::tom::ConstElement<ocs::tom::JB> job,
                                                               uint64_t report_time) {
   DENTER(TOP_LAYER);

   bool ret = true;

   if (host != nullptr) {
      dstring consumable_dstring = DSTRING_INIT;

      /* dump consumables */
      reporting_write_consumables(answer_list, &consumable_dstring,
                                  ocs::tom::get<EH_resource_utilization>(host),
                                  ocs::tom::get<EH_consumable_config_list>(host),
                                  host, job);

      if (sge_dstring_strlen(&consumable_dstring) > 0) {
         dstring host_dstring = DSTRING_INIT;

         sge_dstring_sprintf(&host_dstring, "%s%c" sge_u64 "%c%s%c%s\n",
                             static_cast<const char *>(ocs::tom::get<EH_name>(host)), REPORTING_DELIMITER,
                             (uint64_t)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                             "X", REPORTING_DELIMITER,
                             sge_dstring_get_string(&consumable_dstring));


         /* write record to reporting buffer */
         create_record("host_consumable", sge_dstring_get_string(&host_dstring));
         sge_dstring_free(&host_dstring);
      }

      sge_dstring_free(&consumable_dstring);
   }

   DRETURN(ret);
}

/**
 * @brief Dump sharetree usage
 *
 * @param monitor monitors the use of the global lock
 *
 * @note MT-NOTE: create_sharelog_record() is most probably MT safe
 *       (depends on sge_sharetree_print with uncertain MT safety)
 */
void
ocs::ClassicReportingFileWriter::create_sharelog_record(monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   if (sharelog_interval > 0) {
      dstring prefix_dstring = DSTRING_INIT;
      dstring data_dstring = DSTRING_INIT;
      format_t format;
      char delim[2];
      delim[0] = REPORTING_DELIMITER;
      delim[1] = '\0';

      /* we need a prefix containing the reporting file std fields */
      sge_dstring_sprintf(&prefix_dstring, sge_u64 "%csharelog%c", (uint64_t)sge_gmt64_to_time_t(sge_get_gmt64()),
                          REPORTING_DELIMITER, REPORTING_DELIMITER);

      /* define output format */
      format.name_format = false;
      format.delim = delim;
      format.line_delim = "\n";
      format.rec_delim = "";
      format.str_format = "%s";
      format.field_names = nullptr;
      format.format_times = false;
      format.line_prefix = "sharelog";

      /* dump the sharetree data - only create sharelog entries if we have a sharetree */
      MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_READ), monitor);

      const auto master_stree_list = ocs::DataStore::master_list<SGE_TYPE_SHARETREE>();
      const bool has_sharetree = !master_stree_list.empty();
      if (has_sharetree) {
         const auto master_user_list = ocs::DataStore::master_list<SGE_TYPE_USER>();
         const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
         const auto master_project_list = ocs::DataStore::master_list<SGE_TYPE_PROJECT>();

         sge_sharetree_print(&data_dstring, nullptr, master_stree_list, master_user_list, master_project_list,
                             master_userset_list,
                             true, false, nullptr, &format);
      }

      SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);

      if (has_sharetree) {
         /* write data to reporting buffer */
         // @todo use create_record
         sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
         buffer += sge_dstring_get_string(&data_dstring);
         sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
      }

      /* cleanup */
      sge_dstring_free(&prefix_dstring);
      sge_dstring_free(&data_dstring);
   }

   DRETURN_VOID;
}

/** @brief Append the requested load values as a comma separated `name=value` list
 *
 * Only the variables named in `variables` are written, and only those that are
 * actually reported in `load_list` - a variable that no execution daemon
 * reports is skipped rather than written empty.
 *
 * @param answer_list used to return error messages
 * @param buffer the record being built; the list is appended to it
 * @param load_list the reported load values (`HL_Type`)
 * @param variables the load values to write (`STU_Type`), from the reporting
 *                  configuration
 * @return true on success
 *
 * @note MT-NOTE: write_load_values() is MT-safe
 */
bool
ocs::ClassicReportingFileWriter::write_load_values(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, dstring *buffer,
                                                   const ocs::tom::ConstList<ocs::tom::HL> load_list,
                                                   const ocs::tom::ConstList<ocs::tom::STU> variables) {
   DENTER(TOP_LAYER);
   bool ret = true;
   bool first = true;

   for (const auto variable : variables) {
      const char *name = ocs::tom::get<STU_name>(variable);
      const auto load = load_list.find_first<HL_name>(name);
      if (load) {
         if (first) {
            first = false;
         } else {
            sge_dstring_append_char(buffer, ',');
         }
         sge_dstring_sprintf_append(buffer, "%s=%s",
                                    name, ocs::tom::get<HL_value>(load));
      }

   }

   DRETURN(ret);
}

/**
 * @brief New ar record will be written
 *
 * Flushs the information that into the accounting file that a new
 * advance reservation has been created.
 *
 * @param answer_list answer list
 * @param ar the ar object which has been created
 * @param report_time the corresponding timestamp
 *
 * @return true  success false failure
 *
 * @note MT-NOTE: reporting_flush() is MT-safe
 */
bool
ocs::ClassicReportingFileWriter::create_new_ar_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                      const ocs::tom::ConstElement<ocs::tom::AR> ar,
                                                      uint64_t report_time) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const char *owner = ocs::tom::get<AR_owner>(ar);

   dstring dstr = DSTRING_INIT;
   sge_dstring_sprintf_append(&dstr,
                              sge_u64 "%c"
                              SFN "%c"
                              sge_u64 "%c"
                              sge_u32"%c"
                              "%s\n",
                              (uint64_t)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              "new_ar", REPORTING_DELIMITER,
                              (uint64_t)sge_gmt64_to_time_t(ocs::tom::get<AR_submission_time>(ar)), REPORTING_DELIMITER,
                              ocs::tom::get<AR_id>(ar), REPORTING_DELIMITER,
                              (owner != nullptr) ? owner : "");
   // @todo use create_record
   sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
   buffer += sge_dstring_get_string(&dstr);
   sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);

   sge_dstring_free(&dstr);

   DRETURN(ret);
}

/**
 * @brief Writes ar attributes
 *
 * Writes advance reservation attributes into the reporting file.
 * This will be done whenever the ar settings change and when a new
 * ar object is created.
 *
 * @param answer_list answer list
 * @param ar the ar object which has been created
 * @param report_time the corresponding timestamp
 *
 * @return true  success false failure
 *
 * @note MT-NOTE: reporting_flush() is MT-safe
 */
bool
ocs::ClassicReportingFileWriter::create_ar_attribute_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                            const ocs::tom::ConstElement<ocs::tom::AR> ar,
                                                            uint64_t report_time) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const char *pe_name;
   const char *ar_name;
   const char *ar_account;
   dstring ar_granted_resources = DSTRING_INIT;
   dstring dstr = DSTRING_INIT;

   pe_name = ocs::tom::get<AR_pe>(ar);
   ar_name = ocs::tom::get<AR_name>(ar);
   ar_account = ocs::tom::get<AR_account>(ar);
   centry_list_append_to_dstring(ocs::tom::get<AR_resource_list>(ar), &ar_granted_resources);
   sge_dstring_sprintf_append(&dstr,
                              sge_u64 "%c"
                              SFN "%c"
                              sge_u64 "%c"   /* report_time */
                              sge_u64 "%c"   /* AR_submission_time */
                              sge_u32"%c"   /* AR_id */
                              "%s%c"               /* AR_name */
                              "%s%c"               /* AR_account */
                              sge_u64 "%c"   /* AR_start_time */
                              sge_u64 "%c"   /* AR_end_time */
                              "%s%c"               /* AR_pe */
                              "%s\n",              /* granted resources */
                              (uint64_t)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              "ar_attr", REPORTING_DELIMITER,
                              (uint64_t)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              (uint64_t)sge_gmt64_to_time_t(ocs::tom::get<AR_submission_time>(ar)), REPORTING_DELIMITER,
                              ocs::tom::get<AR_id>(ar), REPORTING_DELIMITER,
                              (ar_name != nullptr) ? ar_name : "", REPORTING_DELIMITER,
                              (ar_account != nullptr) ? ar_account : "", REPORTING_DELIMITER,
                              (uint64_t)sge_gmt64_to_time_t(ocs::tom::get<AR_start_time>(ar)), REPORTING_DELIMITER,
                              (uint64_t)sge_gmt64_to_time_t(ocs::tom::get<AR_end_time>(ar)), REPORTING_DELIMITER,
                              (pe_name != nullptr) ? pe_name : "", REPORTING_DELIMITER,
                              sge_dstring_get_string(&ar_granted_resources));

   // @todo use create_record
   sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
   buffer += sge_dstring_get_string(&dstr);
   sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);

   sge_dstring_free(&ar_granted_resources);
   sge_dstring_free(&dstr);

   DRETURN(ret);
}

/**
 * @brief Writes status change info
 *
 * Writes logging information into the reporting file whenever a status
 * change of an advance reservation occures
 *
 * @param answer_list answer list
 * @param ar the ar object which has been created
 * @param event the event if which caused the state change
 * @param ar_description a human readable description
 * @param report_time the corresponding timestamp
 *
 * @return true  success false failure
 *
 * @note MT-NOTE: reporting_flush() is MT-safe
 */
bool
ocs::ClassicReportingFileWriter::create_ar_log_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                      const ocs::tom::ConstElement<ocs::tom::AR> ar,
                                                      ar_state_event_t event, const char *ar_description,
                                                      uint64_t report_time) {
   DENTER(TOP_LAYER);

   bool ret = true;
   dstring state_string = DSTRING_INIT;
   dstring dstr = DSTRING_INIT;

   ar_state2dstring((ar_state_t) ocs::tom::get<AR_state>(ar), &state_string);
   sge_dstring_sprintf_append(&dstr,
                              sge_u64 "%c"
                              SFN "%c"
                              sge_u64 "%c"   /* report_time */
                              sge_u64 "%c"   /* AR submission time */
                              sge_u32"%c"   /* AR_id */
                              "%s%c"               /* AR_state as string*/
                              "%s%c"               /* event as string*/
                              "%s\n",              /* message */
                              (uint64_t)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              "ar_log", REPORTING_DELIMITER,
                              (uint64_t)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              (uint64_t)sge_gmt64_to_time_t(ocs::tom::get<AR_submission_time>(ar)), REPORTING_DELIMITER,
                              ocs::tom::get<AR_id>(ar), REPORTING_DELIMITER,
                              sge_dstring_get_string(&state_string), REPORTING_DELIMITER,
                              ar_get_string_from_event(event), REPORTING_DELIMITER,
                              (ar_description != nullptr) ? ar_description : "");

   // @todo use create_record
   sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
   buffer += sge_dstring_get_string(&dstr);
   sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);

   sge_dstring_free(&state_string);
   sge_dstring_free(&dstr);

   DRETURN(ret);
}

/**
 * @brief Ar accounting records will be written
 *
 * This records will be written for all reserved qinstance whenever an
 * advance reservation terminates.
 *
 * @param answer_list answer list
 * @param ar the ar object which has been created
 * @param report_time the corresponding timestamp
 *
 * @return true  success false failure
 *
 * @note MT-NOTE: create_ar_acct_records() is MT safe
 *
 * @see #create_single_ar_acct_record
 */
bool ocs::ClassicReportingFileWriter::create_ar_acct_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                            const ocs::tom::ConstElement<ocs::tom::AR> ar,
                                                            uint64_t report_time) {
   DENTER(TOP_LAYER);

   bool ret = true;
   dstring dstr = DSTRING_INIT;

   for (ocs::tom::ConstElement<ocs::tom::JG> elem : ocs::tom::get<AR_granted_slots>(ar)) {
      const char *queue_name = ocs::tom::get<JG_qname>(elem);
      uint32_t slots = ocs::tom::get<JG_slots>(elem);
      dstring cqueue_name = DSTRING_INIT;
      dstring host_or_hgroup = DSTRING_INIT;

      if (!cqueue_name_split(queue_name, &cqueue_name, &host_or_hgroup, nullptr, nullptr)) {
         ret = false;
         continue;
      }

      create_single_ar_acct_record(&dstr, ar, sge_dstring_get_string(&cqueue_name),
                                   sge_dstring_get_string(&host_or_hgroup), slots, report_time);

      sge_dstring_free(&cqueue_name);
      sge_dstring_free(&host_or_hgroup);
   }

   // @todo use create_record
   sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
   buffer += sge_dstring_get_string(&dstr);
   sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);

   sge_dstring_free(&dstr);

   DRETURN(ret);
}

/**
 * @brief Ar accounting record will be written
 *
 * This record will be written for every qinstance whenever an
 * advance reservation terminates.
 *
 * @param dstr the record being built; the accounting line is appended to it
 * @param ar the ar object which has been created
 * @param cqueue_name cluster queue name
 * @param hostname hostname of the qinstance
 * @param slots number of reserved slots
 * @param report_time the corresponding timestamp
 *
 * @note MT-NOTE: create_single_ar_acct_record() is MT-safe
 */
void
ocs::ClassicReportingFileWriter::create_single_ar_acct_record(dstring *dstr,
                                                              const ocs::tom::ConstElement<ocs::tom::AR> ar,
                                                              const char *cqueue_name, const char *hostname,
                                                              uint32_t slots, uint64_t report_time) {
   DENTER(TOP_LAYER);

   sge_dstring_sprintf_append(dstr,
                              sge_u64 "%c"
                              SFN "%c"
                              sge_u64 "%c"   /* report_time */
                              sge_u64 "%c"   /* AR_submission_time */
                              sge_u32"%c"   /* AR_id */
                              "%s%c"               /* cqueue */
                              "%s%c"               /* execution hostname */
                              sge_u32"\n",  /* number of slots */
                              (uint64_t)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              "ar_acct", REPORTING_DELIMITER,
                              (uint64_t)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              (uint64_t)sge_gmt64_to_time_t(ocs::tom::get<AR_submission_time>(ar)), REPORTING_DELIMITER,
                              ocs::tom::get<AR_id>(ar), REPORTING_DELIMITER,
                              cqueue_name, REPORTING_DELIMITER,
                              hostname, REPORTING_DELIMITER,
                              slots);

   DRETURN_VOID;
}
