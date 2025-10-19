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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <ctime>

#include "uti/sge_dstring.h"
#include "uti/sge_lock.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_DataStore.h"
#include "sched/sge_resource_utilization.h"

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
/****** qmaster/reporting/reporting_initialize() ***************************
*  NAME
*     reporting_initialize() -- initialize the reporting module
*
*  SYNOPSIS
*     bool reporting_initialize(lList **answer_list) 
*
*  FUNCTION
*     Register reporting and sharelog trigger as well as the respective event
*     handler.
*
*  INPUTS
*     lList **answer_list - used to return error messages
*
*  RESULT
*     bool - true on success, false on error
*
*  NOTES
*     MT-NOTE: reporting_initialize() is MT safe.
*
*  SEE ALSO
*     qmaster/reporting/reporting_shutdown()
*     Timeeventmanager/te_add()
*******************************************************************************/
void
reporting_initialize() {

   DENTER(TOP_LAYER);

   // create ReportingFileWriter
   ocs::ReportingFileWriter::initialize();

   te_register_event_handler(reporting_trigger_handler, TYPE_REPORTING_TRIGGER);

   /* we always have the reporting trigger for flushing reporting files and
    * checking for new reporting configuration
    */
   te_event_t ev = te_new_event(sge_get_gmt64(), TYPE_REPORTING_TRIGGER, ONE_TIME_EVENT, 1, 0, nullptr);
   te_add_event(ev);
   te_free_event(&ev);
}

void
reporting_reinitialize_timed_event() {
   te_delete_all_one_time_events(TYPE_REPORTING_TRIGGER);
   te_event_t ev = te_new_event(sge_get_gmt64(), TYPE_REPORTING_TRIGGER, ONE_TIME_EVENT, 1, 0, nullptr);
   te_add_event(ev);
   te_free_event(&ev);
}

/****** qmaster/reporting/reporting_shutdown() *****************************
*  NAME
*     reporting_shutdown() -- shutdown the reporting module
*
*  SYNOPSIS
*     bool reporting_shutdown(lList **answer_list, bool do_spool) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     lList **answer_list - used to return error messages
*     bool do_spool       - if set to true changes must be spooled
*                           if set to false don't spool changes
*
*  RESULT
*     bool - true on success, false on error.
*
*  NOTES
*     MT-NOTE: reporting_shutdown() is MT safe
*
*  SEE ALSO
*     qmaster/reporting/reporting_initialize()
*******************************************************************************/
bool
reporting_shutdown(lList **answer_list, bool do_spool) {
   bool ret = true;
   lList *alp = nullptr;

   DENTER(TOP_LAYER);

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

/****** qmaster/reporting/reporting_trigger_handler() **********************
*  NAME
*     reporting_trigger_handler() -- process timed event
*
*  SYNOPSIS
*     void 
*     reporting_trigger_handler(te_event_t anEvent)
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     te_event_t - timed event
*
*  NOTES
*     MT-NOTE: reporting_trigger_handler() is MT safe.
*
*  SEE ALSO
*     Timeeventmanager/te_deliver()
*     Timeeventmanager/te_add()
*******************************************************************************/
void
reporting_trigger_handler(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   u_long64 next_flush = ocs::ReportingFileWriter::trigger_all(monitor);
   te_event_t ev = te_new_event(next_flush, te_get_type(anEvent), ONE_TIME_EVENT, 1, 0, nullptr);
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
} /* reporting_trigger_handler() */

/*
* NOTES
*     MT-NOTE: intermediate_usage_written() is MT-safe
*/
bool
intermediate_usage_written(const lListElem *job_report, const lListElem *ja_task) {
   bool ret = false;
   const char *pe_task_id;
   const lList *reported_usage = nullptr;

   /* do we have a tightly integrated parallel task? */
   pe_task_id = lGetString(job_report, JR_pe_task_id_str);
   if (pe_task_id != nullptr) {
      const lListElem *pe_task = lGetElemStr(lGetList(ja_task, JAT_task_list),
                                             PET_id, pe_task_id);
      if (pe_task != nullptr) {
         reported_usage = lGetList(pe_task, PET_reported_usage);
      }
   } else {
      reported_usage = lGetList(ja_task, JAT_reported_usage_list);
   }

   /* the reported usage list will be created when the first intermediate
    * acct record is written
    */
   if (reported_usage != nullptr) {
      ret = true;
   }

   return ret;
}

// object methods

bool
ocs::ClassicAccountingFileWriter::create_acct_record(lList **answer_list, lListElem *job_report, lListElem *job,
                                                lListElem *ja_task, bool intermediate) {
   DENTER(TOP_LAYER);
   bool ret = true;

   /* accounting records will only be written at job end, not for intermediate
    * reports
    */
   if (!intermediate) {
      // get the job category string
      const lList *master_category_list = *ocs::DataStore::get_master_list(SGE_TYPE_CATEGORY);
      u_long32 category_id = lGetUlong(job, JB_category_id);
      lListElem *category = lGetElemUlongRW(master_category_list, CT_id, category_id);
      const char *category_string = lGetString(category, CT_str);

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

/****** qmaster/reporting/create_acct_record() *******************
*  NAME
*     create_acct_record() -- create an accounting record
*
*  SYNOPSIS
*     bool create_acct_record(lList **answer_list,
*                                       lListElem *job_report,
*                                       lListElem *job, lListElem *ja_task)
*
*  FUNCTION
*     Create an accounting record.
*     Depending on the cluster configuration, parameter reporting_params,
*     accounting is written to the accounting file and/or the reporting file.
*
*     During the runtime of jobs, intermediate accounting records can be written
*     to the reporting file. This is usually done at midnight, to have correct
*     daily accounting information for long running jobs.
*
*  INPUTS
*     lList **answer_list   - used to report error messages
*     lListElem *job_report - job report from execd
*     lListElem *job        - job referenced in report
*     lListElem *ja_task    - array task that finished
*     bool intermediate     - is this an intermediate accounting record?
*
*  RESULT
*     bool - true on success, false on error
*
*  NOTES
*     MT-NOTE: create_acct_record() is not MT safe as the
*              MT safety of called functions sge_build_job_category and
*              sge_write_rusage is not defined.
*******************************************************************************/
/* @todo: we should also pass pe_task. It is known in the code pieces where
 * create_acct_record is called and we needn't search it from ja_task
 */
bool
ocs::ClassicReportingFileWriter::create_acct_record(lList **answer_list, lListElem *job_report, lListElem *job,
                                               lListElem *ja_task, bool intermediate) {
   DENTER(TOP_LAYER);

   // get the job category string
   const lList *master_category_list = *ocs::DataStore::get_master_list(SGE_TYPE_CATEGORY);
   u_long32 category_id = lGetUlong(job, JB_category_id);
   lListElem *category = lGetElemUlongRW(master_category_list, CT_id, category_id);
   const char *category_string = lGetString(category, CT_str);

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

bool
ocs::ClassicReportingFileWriter::create_new_job_record(lList **answer_list, const lListElem *job) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if (do_joblog && job != nullptr) {
      dstring job_dstring = DSTRING_INIT;

      u_long32 job_number, priority;
      u_long64 submission_time;
      const char *job_name, *owner, *group, *project, *department, *account;

      job_number = lGetUlong(job, JB_job_number);
      priority = lGetUlong(job, JB_priority);
      submission_time = sge_gmt64_to_time_t(lGetUlong64(job, JB_submission_time));
      job_name = lGetStringNotNull(job, JB_job_name);
      owner = lGetStringNotNull(job, JB_owner);
      group = lGetStringNotNull(job, JB_group);
      project = lGetStringNotNull(job, JB_project);
      department = lGetStringNotNull(job, JB_department);
      account = lGetStringNotNull(job, JB_account);

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

bool
ocs::ClassicReportingFileWriter::create_job_log(lList **answer_list, u_long64 event_time, const job_log_t type, const char *user,
                                           const char *host, const lListElem *job_report, const lListElem *job, const lListElem *ja_task,
                                           const lListElem *pe_task, const char *message) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if (do_joblog && job != nullptr) {
      dstring job_dstring = DSTRING_INIT;

      u_long32 job_id;
      int ja_task_id = -1;
      const char *pe_task_id = NONE_STR;
      u_long32 state_time = 0, jstate;
      const char *event;
      char state[20];
      u_long32 priority;
      u_long64 submission_time;
      const char *job_name, *owner, *group, *project, *department, *account;

      job_id = lGetUlong(job, JB_job_number);

      /* set ja_task_id:
       * -1, if we don't have a ja_task
       *  0, if we have a non array job
       *  task_number for array jobs
       */
      if (ja_task != nullptr) {
         if (job_is_array(job)) {
            ja_task_id = (int) lGetUlong(ja_task, JAT_task_number);
         } else {
            ja_task_id = 0;
         }
      }
      if (pe_task != nullptr) {
         pe_task_id = lGetStringNotNull(pe_task, PET_id);
      }

      /* JG: TODO: implement the whole job/task state mess */
      if (pe_task != nullptr) {
         jstate = lGetUlong(pe_task, PET_status);
      } else if (ja_task != nullptr) {
         jstate = lGetUlong(ja_task, JAT_status);
      } else {
         jstate = 0;
      }

      event = get_job_log_name(type);

      *state = '\0';
      job_get_state_string(state, jstate);
      if (message == nullptr) {
         message = "";
      }

      priority = lGetUlong(job, JB_priority);
      submission_time = sge_gmt64_to_time_t(lGetUlong64(job, JB_submission_time));
      job_name = lGetStringNotNull(job, JB_job_name);
      owner = lGetStringNotNull(job, JB_owner);
      group = lGetStringNotNull(job, JB_group);
      project = lGetStringNotNull(job, JB_project);
      department = lGetStringNotNull(job, JB_department);
      account = lGetStringNotNull(job, JB_account);

      sge_dstring_sprintf(&job_dstring,
                          sge_u64 "%c%s%c" sge_u32 "%c%d%c%s%c%s%c%s%c%s%c" sge_u32 "%c" sge_u32 "%c" sge_u64 "%c%s%c%s%c%s%c%s%c%s%c%s%c%s\n",
                          (u_long64)sge_gmt64_to_time_t(event_time), REPORTING_DELIMITER,
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

/****** qmaster/reporting/reporting_write_consumables() ********************
*  NAME
*     reporting_write_consumables() -- dump consumables to a buffer
*
*  SYNOPSIS
*     static bool
*     reporting_write_consumables(lList **answer_list, dstring *buffer,
*                                 const lList *actual, const lList *total
*                                 const lListElem *host,
*                                 const lListElem *job)
*
*  FUNCTION
*     ???
*
*  INPUTS
*     lList **answer_list   - used to return error messages
*     dstring *buffer       - target buffer
*     const lList *actual   - actual consumable values
*     const lList *total    - configured consumable values
*     const lListElem *host - host for which to output data
*     const lListElem *job  - optional: job which changes consumables
*
*  RESULT
*     bool - true on success, false on error
*
*  NOTES
*     MT-NOTE: reporting_write_consumables() is MT safe
*******************************************************************************/
void
ocs::ClassicReportingFileWriter::reporting_write_consumables(lList **answer_list, dstring *buffer, const lList *actual, const lList *total,
                            const lListElem *host, const lListElem *job) const {
   DENTER(TOP_LAYER);

   const lListElem *cep;
   for_each_ep(cep, actual) {
      const char *name = lGetString(cep, RUE_name);
      bool log_variable = true;

      /*
       * if log_consumables is false, lookup if the consumable shall be logged
       * due to reporting_variables in global/local host
       */
      if (!log_consumables) {
         const lList *report_variables = lGetList(host, EH_merged_report_variables);
         if (lGetElemStr(report_variables, STU_name, name) == nullptr) {
            log_variable = false;
         } else {
            /*
             * if we log consumables for a specific job, make sure to log only
             * consumables, which are requested by the job
             * slots is an implicit request - always log it if requested
             */
            if (strcmp(name, SGE_ATTR_SLOTS) != 0 && job != nullptr) {
               if (job_get_request(job, name) == nullptr) {
                  log_variable = false;
               }
            }
         }
      }

      /* now do the logging, if requested */
      if (log_variable) {
         const lListElem *tep = lGetElemStr(total, CE_name, name);
         if (tep != nullptr) {
            sge_dstring_append(buffer, name);
            sge_dstring_append_char(buffer, '=');
            utilization_print_to_dstring(cep, buffer);
            sge_dstring_append_char(buffer, '=');
            centry_print_resource_to_dstring(tep, buffer);

            if (lNext(cep) != nullptr) {
               sge_dstring_append_char(buffer, ',');
            }
         }
      }
   }

   DRETURN_VOID;
}

/****** qmaster/reporting/create_queue_record() *******************
*  NAME
*     create_queue_record() -- create queue reporting record
*
*  SYNOPSIS
*     bool
*     create_queue_record(lList **answer_list,
*                                  const lListElem *queue,
*                                  u_long32 report_time)
*
*  FUNCTION
*     ???
*
*  INPUTS
*     lList **answer_list   - used to return error messages
*     const lListElem *queue - the queue to output
*     u_long32 report_time  - time of the last load report
*
*  RESULT
*     bool - true on success, false on error
*
*  NOTES
*     MT-NOTE: create_queue_record() is MT safe
*******************************************************************************/
bool
ocs::ClassicReportingFileWriter::create_queue_record(lList **answer_list,
                                                const lListElem *queue,
                                                u_long64 report_time) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if (queue != nullptr) {
      dstring queue_dstring = DSTRING_INIT;

      sge_dstring_sprintf(&queue_dstring, "%s%c%s%c" sge_u64 "%c",
                          lGetString(queue, QU_qname),
                          REPORTING_DELIMITER,
                          lGetHost(queue, QU_qhostname),
                          REPORTING_DELIMITER,
                          (u_long64)sge_gmt64_to_time_t(report_time),
                          REPORTING_DELIMITER);
      qinstance_state_append_to_dstring(queue, &queue_dstring);
      sge_dstring_append_char(&queue_dstring, '\n');

      /* write record to reporting buffer */
      create_record("queue", sge_dstring_get_string(&queue_dstring));

      sge_dstring_free(&queue_dstring);
   }

   DRETURN(ret);
}

/****** qmaster/reporting/create_queue_consumable_record() ********
*  NAME
*     create_queue_consumable_record() -- write queue consumables
*
*  SYNOPSIS
*     bool
*     create_queue_consumable_record(lList **answer_list,
*                                              const lListElem *host,
*                                              const lListElem *queue,
*                                              const lListElem *job,
*                                              u_long32 report_time)
*
*  FUNCTION
*     ???
*
*  INPUTS
*     lList **answer_list    - used to return error messages
*     const lListElem *host  - host on which the qinstance is located
*     const lListElem *queue - queue instance to output
*     const lListElem *job   - optional: job which changes consumables
*     u_long32 report_time   - time when consumables changed
*
*  RESULT
*     bool - true on success, false on error
*
*  NOTES
*     MT-NOTE: create_queue_consumable_record() is MT safe
*******************************************************************************/
bool
ocs::ClassicReportingFileWriter::create_queue_consumable_record(lList **answer_list,
                                                           const lListElem *host,
                                                           const lListElem *queue,
                                                           const lListElem *job,
                                                           u_long64 report_time) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if (host != nullptr && queue != nullptr) {
      dstring consumable_dstring = DSTRING_INIT;

      /* dump consumables */
      reporting_write_consumables(answer_list, &consumable_dstring,
                                  lGetList(queue, QU_resource_utilization),
                                  lGetList(queue, QU_consumable_config_list),
                                  host, job);

      if (sge_dstring_strlen(&consumable_dstring) > 0) {
         dstring queue_dstring = DSTRING_INIT;
         sge_dstring_sprintf(&queue_dstring, "%s%c%s%c" sge_u64 "%c",
                             lGetString(queue, QU_qname),
                             REPORTING_DELIMITER,
                             lGetHost(queue, QU_qhostname),
                             REPORTING_DELIMITER,
                             (u_long64)sge_gmt64_to_time_t(report_time),
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

/****** qmaster/reporting/create_host_record() *******************
*  NAME
*     create_host_record() -- create host reporting record
*
*  SYNOPSIS
*     bool
*     create_host_record(lList **answer_list,
*                                  const lListElem *host,
*                                  u_long32 report_time)
*
*  FUNCTION
*     ???
*
*  INPUTS
*     lList **answer_list   - used to return error messages
*     const lListElem *host - the host to output
*     u_long32 report_time  - time of the last load report
*
*  RESULT
*     bool - true on success, false on error
*
*  NOTES
*     MT-NOTE: create_host_record() is MT safe
*******************************************************************************/
bool
ocs::ClassicReportingFileWriter::create_host_record(lList **answer_list,
                                               const lListElem *host,
                                               u_long64 report_time) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if (host != nullptr) {
      dstring load_dstring = DSTRING_INIT;

      write_load_values(answer_list, &load_dstring, lGetList(host, EH_load_list),
                        lGetList(host, EH_merged_report_variables));

      /* As long as we have no host status information, dump host data only if we have
       * load values to report.
       */
      if (sge_dstring_strlen(&load_dstring) > 0) {
         dstring host_dstring = DSTRING_INIT;
         sge_dstring_sprintf(&host_dstring, "%s%c" sge_u64 "%c%s%c%s\n",
                             lGetHost(host, EH_name), REPORTING_DELIMITER,
                             (u_long64)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
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

/****** qmaster/reporting/create_host_consumable_record() ********
*  NAME
*     create_host_consumable_record() -- write host consumables
*
*  SYNOPSIS
*     bool
*     create_host_consumable_record(lList **answer_list,
*                                             const lListElem *host,
*                                             const lListElem *job,
*                                             u_long32 report_time)
*
*  FUNCTION
*     ???
*
*  INPUTS
*     lList **answer_list   - used to return error messages
*     const lListElem *host - host to output
*     const lListElem *job  - optional: job which changes consumables
*     u_long32 report_time  - time when consumables changed
*
*  RESULT
*     bool - true on success, false on error
*
*  NOTES
*     MT-NOTE: create_host_consumable_record() is MT safe
*******************************************************************************/
bool
ocs::ClassicReportingFileWriter::create_host_consumable_record(lList **answer_list,
                                                          const lListElem *host,
                                                          const lListElem *job,
                                                          u_long64 report_time) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if (host != nullptr) {
      dstring consumable_dstring = DSTRING_INIT;

      /* dump consumables */
      reporting_write_consumables(answer_list, &consumable_dstring,
                                  lGetList(host, EH_resource_utilization),
                                  lGetList(host, EH_consumable_config_list),
                                  host, job);

      if (sge_dstring_strlen(&consumable_dstring) > 0) {
         dstring host_dstring = DSTRING_INIT;

         sge_dstring_sprintf(&host_dstring, "%s%c" sge_u64 "%c%s%c%s\n",
                             lGetHost(host, EH_name), REPORTING_DELIMITER,
                             (u_long64)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
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

/****** qmaster/reporting/create_sharelog_record() ***************
*  NAME
*     create_sharelog_record() -- dump sharetree usage
*
*  SYNOPSIS
*     bool
*     create_sharelog_record(lList **answer_list)
*
*  FUNCTION
*     ???
*
*  INPUTS
*     lList **answer_list   - used to return error messages
*     monitoring_t *monitor - monitors the use of the global lock
*
*  RESULT
*     bool -  true on success, false on error
*
*  NOTES
*     MT-NOTE: create_sharelog_record() is most probably MT safe
*              (depends on sge_sharetree_print with uncertain MT safety)
*******************************************************************************/
void
ocs::ClassicReportingFileWriter::create_sharelog_record(monitoring_t *monitor) {
   const lList *master_stree_list = *ocs::DataStore::get_master_list(SGE_TYPE_SHARETREE);
   const lList *master_user_list = *ocs::DataStore::get_master_list(SGE_TYPE_USER);
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);

   DENTER(TOP_LAYER);

   if (sharelog_interval > 0) {
      /* only create sharelog entries if we have a sharetree */
      if (lGetNumberOfElem(master_stree_list) > 0) {
         dstring prefix_dstring = DSTRING_INIT;
         dstring data_dstring = DSTRING_INIT;
         format_t format;
         char delim[2];
         delim[0] = REPORTING_DELIMITER;
         delim[1] = '\0';

         /* we need a prefix containing the reporting file std fields */
         sge_dstring_sprintf(&prefix_dstring, sge_u64 "%csharelog%c", (u_long64)sge_gmt64_to_time_t(sge_get_gmt64()),
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

         /* dump the sharetree data */
         MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_READ), monitor);

         sge_sharetree_print(&data_dstring, nullptr, master_stree_list, master_user_list, master_project_list,
                             master_userset_list,
                             true, false, nullptr, &format);

         SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);

         /* write data to reporting buffer */
         // @todo use create_record
         sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
         buffer += sge_dstring_get_string(&data_dstring);
         sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);

         /* cleanup */
         sge_dstring_free(&prefix_dstring);
         sge_dstring_free(&data_dstring);
      }
   }
}

/*
* NOTES
*     MT-NOTE: reporting_write_load_values() is MT-safe
*/
bool
ocs::ClassicReportingFileWriter::write_load_values(lList **answer_list, dstring *buffer,
                                              const lList *load_list, const lList *variables) {
   bool ret = true;
   bool first = true;
   const lListElem *variable;

   DENTER(TOP_LAYER);

   for_each_ep(variable, variables) {
      const char *name;
      const lListElem *load;

      name = lGetString(variable, STU_name);
      load = lGetElemStr(load_list, HL_name, name);
      if (load != nullptr) {
         if (first) {
            first = false;
         } else {
            sge_dstring_append_char(buffer, ',');
         }
         sge_dstring_sprintf_append(buffer, "%s=%s",
                                    name, lGetString(load, HL_value));
      }

   }

   DRETURN(ret);
}

/****** qmaster/create_new_ar_record() ******************************
*  NAME
*     create_new_ar_record() -- new ar record will be written
*
*  SYNOPSIS
*     bool
*     create_new_ar_record(lList **answer_list,
*                                    const lListElem *ar,
*                                    u_long32 report_time)
*
*  FUNCTION
*     Flushs the information that into the accounting file that a new
*     advance reservation has been created.
*
*  INPUTS
*     lList **answer_list  - answer list
*     const lListElem *ar  - the ar object which has been created
*     u_long32 report_time - the corresponding timestamp
*
*  RESULT
*     int - true  success
*           false failure
*
*  NOTES
*     MT-NOTE: reporting_flush() is MT-safe
*******************************************************************************/
bool
ocs::ClassicReportingFileWriter::create_new_ar_record(lList **answer_list,
                                                 const lListElem *ar,
                                                 u_long64 report_time) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const char *owner = lGetString(ar, AR_owner);

   dstring dstr = DSTRING_INIT;
   sge_dstring_sprintf_append(&dstr,
                              sge_u64 "%c"
                              SFN "%c"
                              sge_u64 "%c"
                              sge_u32"%c"
                              "%s\n",
                              (u_long64)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              "new_ar", REPORTING_DELIMITER,
                              (u_long64)sge_gmt64_to_time_t(lGetUlong64(ar, AR_submission_time)), REPORTING_DELIMITER,
                              lGetUlong(ar, AR_id), REPORTING_DELIMITER,
                              (owner != nullptr) ? owner : "");
   // @todo use create_record
   sge_mutex_lock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);
   buffer += sge_dstring_get_string(&dstr);
   sge_mutex_unlock(typeid(*this).name(), __func__, __LINE__, &buffer_mutex);

   sge_dstring_free(&dstr);

   DRETURN(ret);
}

/****** qmaster/create_ar_attribute_record() ************************
*  NAME
*     create_ar_attribute_record() -- writes ar attributes
*
*  SYNOPSIS
*     bool
*     create_ar_attribute_record(lList **answer_list,
*                                          const lListElem *ar,
*                                          u_long32 report_time)
*
*  FUNCTION
*     Writes advance reservation attributes into the reporting file.
*     This will be done whenever the ar settings change and when a new
*     ar object is created.
*
*  INPUTS
*     lList **answer_list  - answer list
*     const lListElem *ar  - the ar object which has been created
*     u_long32 report_time - the corresponding timestamp
*
*  RESULT
*     int - true  success
*           false failure
*
*  NOTES
*     MT-NOTE: reporting_flush() is MT-safe
*******************************************************************************/
bool
ocs::ClassicReportingFileWriter::create_ar_attribute_record(lList **answer_list,
                                                       const lListElem *ar,
                                                       u_long64 report_time) {
   bool ret = true;
   const char *pe_name;
   const char *ar_name;
   const char *ar_account;
   dstring ar_granted_resources = DSTRING_INIT;
   dstring dstr = DSTRING_INIT;

   DENTER(TOP_LAYER);

   pe_name = lGetString(ar, AR_pe);
   ar_name = lGetString(ar, AR_name);
   ar_account = lGetString(ar, AR_account);
   centry_list_append_to_dstring(lGetList(ar, AR_resource_list), &ar_granted_resources);
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
                              (u_long64)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              "ar_attr", REPORTING_DELIMITER,
                              (u_long64)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              (u_long64)sge_gmt64_to_time_t(lGetUlong64(ar, AR_submission_time)), REPORTING_DELIMITER,
                              lGetUlong(ar, AR_id), REPORTING_DELIMITER,
                              (ar_name != nullptr) ? ar_name : "", REPORTING_DELIMITER,
                              (ar_account != nullptr) ? ar_account : "", REPORTING_DELIMITER,
                              (u_long64)sge_gmt64_to_time_t(lGetUlong64(ar, AR_start_time)), REPORTING_DELIMITER,
                              (u_long64)sge_gmt64_to_time_t(lGetUlong64(ar, AR_end_time)), REPORTING_DELIMITER,
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

/****** qmaster/create_ar_log_record() ******************************
*  NAME
*     create_ar_log_record() -- writes status change info
*
*  SYNOPSIS
*     bool
*     create_ar_log_record(lList **answer_list,
*                                    const lListElem *ar,
*                                    ar_state_event_t event,
*                                    const char *ar_description,
*                                    u_long32 report_time)
*
*  FUNCTION
*     Writes logging information into the reporting file whenever a status
*     change of an advance reservation occures
*
*  INPUTS
*     lList **answer_list  - answer list
*     const lListElem *ar  - the ar object which has been created
*     ar_state_event_t event  - the event if which caused the state change
*     const char *ar_description  - a human readable description
*     u_long32 report_time - the corresponding timestamp
*
*  RESULT
*     int - true  success
*           false failure
*
*  NOTES
*     MT-NOTE: reporting_flush() is MT-safe
*******************************************************************************/
bool
ocs::ClassicReportingFileWriter::create_ar_log_record(lList **answer_list,
                                                 const lListElem *ar,
                                                 ar_state_event_t event,
                                                 const char *ar_description,
                                                 u_long64 report_time) {
   bool ret = true;
   dstring state_string = DSTRING_INIT;
   dstring dstr = DSTRING_INIT;

   DENTER(TOP_LAYER);

   ar_state2dstring((ar_state_t) lGetUlong(ar, AR_state), &state_string);
   sge_dstring_sprintf_append(&dstr,
                              sge_u64 "%c"
                              SFN "%c"
                              sge_u64 "%c"   /* report_time */
                              sge_u64 "%c"   /* AR submission time */
                              sge_u32"%c"   /* AR_id */
                              "%s%c"               /* AR_state as string*/
                              "%s%c"               /* event as string*/
                              "%s\n",              /* message */
                              (u_long64)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              "ar_log", REPORTING_DELIMITER,
                              (u_long64)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              (u_long64)sge_gmt64_to_time_t(lGetUlong64(ar, AR_submission_time)), REPORTING_DELIMITER,
                              lGetUlong(ar, AR_id), REPORTING_DELIMITER,
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

/****** sge_reporting_qmaster/create_ar_acct_records() ***************
*  NAME
*     create_ar_acct_records() -- ar accounting records will be written
*
*  SYNOPSIS
*     bool create_ar_acct_records(lList **answer_list, const
*     lListElem *ar, u_long32 report_time)
*
*  FUNCTION
*     This records will be written for all reserved qinstance whenever an
*     advance reservation terminates.
*
*  INPUTS
*     lList **answer_list  - answer list
*     const lListElem *ar  - the ar object which has been created
*     u_long32 report_time - the corresponding timestamp
*
*  RESULT
*     int - true  success
*           false failure
*
*  NOTES
*     MT-NOTE: create_ar_acct_records() is MT safe
*
*  SEE ALSO
*     qmaster/create_single_ar_acct_record()
*******************************************************************************/
bool ocs::ClassicReportingFileWriter::create_ar_acct_record(lList **answer_list, const lListElem *ar, u_long64 report_time) {
   bool ret = true;
   dstring dstr = DSTRING_INIT;

   const lListElem *elem;
   for_each_ep(elem, lGetList(ar, AR_granted_slots)) {
      const char *queue_name = lGetString(elem, JG_qname);
      u_long32 slots = lGetUlong(elem, JG_slots);
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

   return ret;
}

/****** qmaster/create_single_ar_acct_record() *****************************
*  NAME
*     create_ar_log_record() -- ar accounting record will be written
*
*  SYNOPSIS
*     bool
*     create_single_ar_acct_record(lList **answer_list,
*                                     const lListElem *ar,
*                                     const char *cqueue_name,
*                                     const char *hostname,
*                                     u_long32 slots,
*                                     u_long32 report_time)
*
*  FUNCTION
*     This record will be written for every qinstance whenever an
*     advance reservation terminates.
*
*  INPUTS
*     lList **answer_list     - answer list
*     const lListElem *ar     - the ar object which has been created
*     const char *cqueue_name - cluster queue name
*     const char *hostname    - hostname of the qinstance
*     u_long32 slots          - number of reserved slots
*     u_long32 report_time    - the corresponding timestamp
*
*  RESULT
*     int - true  success
*           false failure
*
*  NOTES
*     MT-NOTE: reporting_flush() is MT-safe
*******************************************************************************/
void
ocs::ClassicReportingFileWriter::create_single_ar_acct_record(dstring *dstr,
                                                         const lListElem *ar,
                                                         const char *cqueue_name,
                                                         const char *hostname,
                                                         u_long32 slots,
                                                         u_long64 report_time) {
   sge_dstring_sprintf_append(dstr,
                              sge_u64 "%c"
                              SFN "%c"
                              sge_u64 "%c"   /* report_time */
                              sge_u64 "%c"   /* AR_submission_time */
                              sge_u32"%c"   /* AR_id */
                              "%s%c"               /* cqueue */
                              "%s%c"               /* execution hostname */
                              sge_u32"\n",  /* number of slots */
                              (u_long64)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              "ar_acct", REPORTING_DELIMITER,
                              (u_long64)sge_gmt64_to_time_t(report_time), REPORTING_DELIMITER,
                              (u_long64)sge_gmt64_to_time_t(lGetUlong64(ar, AR_submission_time)), REPORTING_DELIMITER,
                              lGetUlong(ar, AR_id), REPORTING_DELIMITER,
                              cqueue_name, REPORTING_DELIMITER,
                              hostname, REPORTING_DELIMITER,
                              slots);
}
