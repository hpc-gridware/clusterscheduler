#pragma once
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
 *  Portions of this software are Copyright (c) 2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The classic delimiter-separated accounting and reporting files
 */

#include "uti/sge_dstring.h"
#include "uti/sge_monitor.h"

#include "tom/ocs_Types.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/sge_advance_reservation.h"

#include "sgeobj/sge_daemonize.h"

#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_HL.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "sgeobj/tom/ocs_tom_STU.h"

#include "ocs_BaseAccountingFileWriter.h"
#include "ocs_BaseReportingFileWriter.h"
#include "sge_qmaster_timed_event.h"
#include "sgeobj/sge_answer.h"

void
reporting_initialize();
void
reporting_reinitialize_timed_event();

bool
reporting_shutdown(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, bool do_spool);

void
reporting_trigger_handler(te_event_t anEvent, monitoring_t *monitor);

bool
intermediate_usage_written(ocs::tom::ConstElement<ocs::tom::JR> job_report,
                           ocs::tom::ConstElement<ocs::tom::JAT> ja_task);

namespace ocs {
   /** @brief Writes the reporting records in the classic delimiter-separated format
    *
    * The format the older reporting tools read: one record per line, fields
    * separated by #REPORTING_DELIMITER.
    */
   class ClassicReportingFileWriter : public BaseReportingFileWriter {
   private:
      static const char REPORTING_DELIMITER{':'};
   public:
      ClassicReportingFileWriter() : BaseReportingFileWriter(bootstrap_get_reporting_file(), true) {
      }

      void
      create_record(const char *type, const char *data);

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
      static void
      create_single_ar_acct_record(dstring *dstr, ocs::tom::ConstElement<ocs::tom::AR> ar, const char *cqueue_name,
                                   const char *hostname, uint32_t slots, uint64_t report_time);

      bool
      write_load_values(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, dstring *buffer,
                        ocs::tom::ConstList<ocs::tom::HL> load_list, ocs::tom::ConstList<ocs::tom::STU> variables);
      void
      reporting_write_consumables(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, dstring *buffer,
                                  ocs::tom::ConstList<ocs::tom::RUE> actual,
                                  ocs::tom::ConstList<ocs::tom::CE> total, ocs::tom::ConstElement<ocs::tom::EH> host,
                                  ocs::tom::ConstElement<ocs::tom::JB> job) const;
   };

   /** @brief Writes the accounting records in the classic delimiter-separated format
    *
    * This is the file `qacct` reads by default.
    */
   class ClassicAccountingFileWriter : public BaseAccountingFileWriter {
   private:
      static const char REPORTING_DELIMITER = ':';
   public:
      ClassicAccountingFileWriter() : BaseAccountingFileWriter(bootstrap_get_acct_file(), true) {
      }

      bool
      create_acct_record(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::Element<ocs::tom::JR> job_report,
                         ocs::tom::Element<ocs::tom::JB> job, ocs::tom::Element<ocs::tom::JAT> ja_task,
                         bool intermediate) override;

   };
}
