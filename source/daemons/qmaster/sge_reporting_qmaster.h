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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_dstring.h"
#include "uti/sge_monitor.h"

#include "cull/cull.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/sge_advance_reservation.h"

#include "sgeobj/sge_daemonize.h"

#include "oge_BaseAccountingFileWriter.h"
#include "oge_BaseReportingFileWriter.h"
#include "sge_qmaster_timed_event.h"

void
reporting_initialize();
void
reporting_reinitialize_timed_event();

bool
reporting_shutdown(lList **answer_list, bool do_spool);

void
reporting_trigger_handler(te_event_t anEvent, monitoring_t *monitor);

bool
intermediate_usage_written(const lListElem *job_report, const lListElem *ja_task);

namespace oge {
   class ClassicReportingFileWriter : public BaseReportingFileWriter {
   private:
      static const char REPORTING_DELIMITER{':'};
   public:
      ClassicReportingFileWriter() : BaseReportingFileWriter(bootstrap_get_reporting_file()) {
      }

      void
      create_record(const char *type, const char *data);

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
      static void
      create_single_ar_acct_record(dstring *dstr, const lListElem *ar, const char *cqueue_name,
                                   const char *hostname, u_long32 slots, u_long32 report_time);

      bool
      write_load_values(lList **answer_list, dstring *buffer,
                        const lList *load_list, const lList *variables);
      void
      reporting_write_consumables(lList **answer_list, dstring *buffer, const lList *actual, const lList *total,
                                  const lListElem *host, const lListElem *job) const;
   };

   class ClassicAccountingFileWriter : public BaseAccountingFileWriter {
   private:
      static const char REPORTING_DELIMITER = ':';
   public:
      ClassicAccountingFileWriter() : BaseAccountingFileWriter(bootstrap_get_acct_file()) {
      }

      bool
      create_acct_record(lList **answer_list, lListElem *job_report, lListElem *job,
                         lListElem *ja_task, bool intermediate) override;

   };
}
