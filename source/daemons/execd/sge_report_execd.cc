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
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Assembling and sending the daemon's periodic reports
 */
#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_report.h"
#include "sgeobj/sge_load.h"
#include "sgeobj/tom/ocs_tom_LR.h"
#include "sgeobj/tom/ocs_tom_REP.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "gdi/ocs_gdi_ClientBase.h"
#include "gdi/ocs_gdi_ClientExecd.h"

#include "comm/commlib.h"

#include "job_report_execd.h"
#include "execd.h"
#include "sge.h"
#include "sge_report_execd.h"

#ifndef NO_SGE_COMPILE_DEBUG
static const char* report_types[] = {
   " REPORT_LOAD",
   " REPORT_EVENTS",
   " REPORT_CONF",
   " REPORT_LICENSE",
   " REPORT_JOB"
};
#endif


extern lUlong sge_execd_report_seqno;


/**
 * @brief Move the next send time of all report sources to a given time at the latest.
 *
 * A report source schedules its next report with the load_report_time valid when it sends one.
 * When load_report_time is lowered, sge_qmaster expects reports in the new interval right away,
 * so the reports scheduled with the old interval must not be sent later than the new interval.
 *
 * @param[in] latest the latest next send time, e.g. now + load_report_time
 * @param[in,out] report_sources the report sources, terminated by an entry with type 0
 */
void
sge_limit_report_times(uint64_t latest, report_source *report_sources) {
   DENTER(TOP_LAYER);
   for (int i = 0; report_sources[i].type; i++) {
      if (report_sources[i].next_send > latest) {
         report_sources[i].next_send = latest;
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Send whichever reports are due
 * @param now the current time
 * @param which a mask of the report types to consider, 0 for all of them
 * @param report_sources the table of report kinds
 * @return 0 on success, 1 if qmaster is not connected since the last registration
 */
int sge_send_all_reports(uint64_t now, int which, report_source *report_sources) {
   DENTER(TOP_LAYER);

   int ret = 0;
   unsigned long connect_time = 0;
   /*
    * Send reports only if there is not a communication error.
    * Don't reset stored communication errors. 
    */

   cl_commlib_get_connect_time(cl_com_get_handle(component_get_component_name(), 0),
                               (char *)ocs::gdi::ClientBase::gdi_get_act_master_host(true), to_cstr(QMASTER), 1,
                               &connect_time);

   if (connect_time != 0 && get_last_qmaster_register_time() >= sge_gmt32_to_gmt64(connect_time)) {
      if (!ocs::gdi::ClientBase::sge_get_com_error_flag(EXECD, ocs::gdi::SGE_COM_WAS_COMMUNICATION_ERROR, false)) {
         const char *master_host = nullptr;
         int i = 0;

         master_host = ocs::gdi::ClientBase::gdi_get_act_master_host(false);
         DPRINTF("SENDING LOAD AND REPORTS\n");
         const auto report_list = ocs::tom::ListPtr<ocs::tom::REP>::create("report list");
         for (i = 0; report_sources[i].type; i++) {
            if (!which || which == report_sources[i].type) {
               DPRINTF("%s\n", report_types[report_sources[i].type - 1]);
               report_sources[i].func(*report_list, now, &(report_sources[i].next_send));
            }
         }

         /* send load report asynchron to qmaster */
         if (!report_list->empty()) {
            /* wrap around */
            if (++sge_execd_report_seqno == 10000) {
               sge_execd_report_seqno = 0;
            }
            ocs::gdi::ClientExecd::report_list_send(*report_list, master_host, to_cstr(QMASTER), 1, 0);
         }
      }
   } else {
      ret = 1;
   }
   DRETURN(ret);
}

/* ----------------------------------------
 
   add a double value to the load report list lpp 
 
*/
/** @brief Add one floating point load value to a report
 * @param[in,out] lpp the report being assembled
 * @param name the load value's name
 * @param value its value
 * @param host the host it belongs to
 * @param units the unit to report it in, or nullptr
 * @return 0 on success
 */
int sge_add_double2load_report(ocs::tom::ListPtr<ocs::tom::LR> &lpp, const char *name, const double value,
                               const char *host, const char *units) {
   DENTER(BASIS_LAYER);

   char load_string[255];
 
   snprintf(load_string, sizeof(load_string), "%f%s", value, units?units:"");
   sge_add_str2load_report(lpp, name, load_string, host);

   DRETURN(0);
}

/* ----------------------------------------

   add an integer value to the load report list lpp

*/
/** @brief Add one integer load value to a report
 * @param[in,out] lpp the report being assembled
 * @param name the load value's name
 * @param value its value
 * @param host the host it belongs to
 * @return 0 on success
 */
int sge_add_int2load_report(ocs::tom::ListPtr<ocs::tom::LR> &lpp, const char *name, const int value,
                            const char *host) {
   DENTER(BASIS_LAYER);

   char load_string[255];
   int ret;

   snprintf(load_string, sizeof(load_string), "%d", value);
   ret = sge_add_str2load_report(lpp, name, load_string, host);

   DRETURN(ret);
}

/* ----------------------------------------

   add a string value to the load report list lpp

*/
/** @brief Add one string load value to a report
 * @param[in,out] lpp the report being assembled; created if it is empty
 * @param name the load value's name
 * @param value its value
 * @param host the host it belongs to
 * @return 0 on success, -1 if an argument is nullptr
 */
int sge_add_str2load_report(ocs::tom::ListPtr<ocs::tom::LR> &lpp, const char *name, const char *value,
                            const char *host) {
   DENTER(BASIS_LAYER);

   if (name == nullptr || value == nullptr || host == nullptr) {
      DRETURN(-1);
   }
   if (!lpp) {
      // created as the free emplace_back() creates a missing list
      lpp = ocs::tom::ListPtr<ocs::tom::LR>::create("");
   }

   DRETURN(sge_add_str2load_report(*lpp, name, value, host));
}

/** @brief Add one string load value to an existing report
 * @param lp the report being assembled
 * @param name the load value's name
 * @param value its value
 * @param host the host it belongs to
 * @return 0 on success, -1 if an argument is nullptr or the report is an empty handle
 */
int sge_add_str2load_report(const ocs::tom::List<ocs::tom::LR> lp, const char *name, const char *value,
                            const char *host) {
   DENTER(BASIS_LAYER);

   ocs::tom::Element<ocs::tom::LR> ep = nullptr;
   const void *iterator = nullptr;

   if (!lp || name == nullptr || value == nullptr || host == nullptr) {
      DRETURN(-1);
   }

   ocs::tom::Element<ocs::tom::LR> search_ep = ocs::tom::find_first_rw<LR_host>(lp, host, &iterator);

   while (search_ep != nullptr) {
      DPRINTF("---> %s\n", ocs::tom::get<LR_name>(search_ep));
      if (strcmp(ocs::tom::get<LR_name>(search_ep), name) == 0) {
         ep = search_ep;
         break;
      }
      search_ep = ocs::tom::find_next_rw<LR_host>(lp, host, &iterator);
   }
   
   if (ep == nullptr) {
      DPRINTF("adding new load variable %s for host %s\n", name, host);
      ep = lp.emplace_back<LR_name>(name);
      ocs::tom::set<LR_host>(ep, host);
      ocs::tom::set<LR_global>(ep, (uint32_t)(strcmp(host, SGE_GLOBAL_NAME) == 0 ? 1 : 0));
      ocs::tom::set<LR_is_static>(ep, sge_is_static_load_value(name));
   }

   ocs::tom::set<LR_value>(ep, value);

   DPRINTF("load value %s for host %s: %s\n", name, host, value);

   DRETURN(0);
}
