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
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstring>

#include "uti/sge_bootstrap.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/sge_load.h"

#include "comm/commlib.h"

#include "job_report_execd.h"
#include "load_avg.h"
#include "execd.h"
#include "sge.h"
#include "sge_report_execd.h"

#ifndef NO_SGE_COMPILE_DEBUG
static char* report_types[] = {
   " REPORT_LOAD",
   " REPORT_EVENTS",
   " REPORT_CONF",
   " REPORT_LICENSE",
   " REPORT_JOB"
};
#endif


extern lUlong sge_execd_report_seqno;


/*-------------------------------------------------------------------------*/
int sge_send_all_reports(u_long32 now, int which, report_source *report_sources)
{
   int ret = 0;
   unsigned long connect_time = 0;
   DENTER(TOP_LAYER);

   /*
    * Send reports only if there is not a communication error.
    * Don't reset stored communication errors. 
    */

   cl_commlib_get_connect_time(cl_com_get_handle(component_get_component_name(), 0),
                               (char *)gdi3_get_act_master_host(true), (char*)prognames[QMASTER], 1,
                               &connect_time);

   if (get_last_qmaster_register_time() >= connect_time && connect_time != 0) {
      if (sge_get_com_error_flag(EXECD, SGE_COM_WAS_COMMUNICATION_ERROR, false) == false) {
         const char *master_host = nullptr;
         lList *report_list = nullptr;
         int i = 0;

         master_host = gdi3_get_act_master_host(false);
         DPRINTF(("SENDING LOAD AND REPORTS\n"));
         report_list = lCreateList("report list", REP_Type);
         for (i = 0; report_sources[i].type; i++) {
            if (!which || which == report_sources[i].type) {
               DPRINTF(("%s\n", report_types[report_sources[i].type - 1]));
               report_sources[i].func(report_list, now, &(report_sources[i].next_send));
            }
         }

         /* send load report asynchron to qmaster */
         if (lGetNumberOfElem(report_list) > 0) {
            /* wrap around */
            if (++sge_execd_report_seqno == 10000) {
               sge_execd_report_seqno = 0;
            }
            report_list_send(report_list, master_host, prognames[QMASTER], 1, 0);
         }
         lFreeList(&report_list);
      }
   } else {
      ret = 1;
   }
   DRETURN(ret);
}

/* ----------------------------------------
 
   add a double value to the load report list lpp 
 
*/
int sge_add_double2load_report(lList **lpp, char *name, double value,
                               const char *host, char *units)
{
   char load_string[255];
 
   DENTER(BASIS_LAYER);
 
   sprintf(load_string, "%f%s", value, units?units:"");
   sge_add_str2load_report(lpp, name, load_string, host);

   DRETURN(0); 
}

/* ----------------------------------------

   add an integer value to the load report list lpp

*/
int sge_add_int2load_report(lList **lpp, const char *name, int value,
                            const char *host)
{
   char load_string[255];
   int ret;

   DENTER(BASIS_LAYER);

   sprintf(load_string, "%d", value);
   ret = sge_add_str2load_report(lpp, name, load_string, host);

   DRETURN(ret);
}

/* ----------------------------------------

   add a string value to the load report list lpp

*/
int sge_add_str2load_report(lList **lpp, const char *name, const char *value, const char *host)
{
   lListElem *ep = nullptr;
   const void *iterator = nullptr;

   DENTER(BASIS_LAYER);

   if (lpp == nullptr || name == nullptr || value == nullptr || host == nullptr) {
      DRETURN(-1);
   }

   if (*lpp != nullptr) {
      lListElem *search_ep = lGetElemHostFirstRW(*lpp, LR_host, host, &iterator);

      while (search_ep != nullptr) {
         DPRINTF(("---> %s\n", lGetString(search_ep, LR_name)));
         if (strcmp(lGetString(search_ep, LR_name), name) == 0) {
            ep = search_ep;
            break;
         }
         search_ep = lGetElemHostNextRW(*lpp, LR_host, host, &iterator);
      }
   }
   
   if (ep == nullptr) {
      DPRINTF(("adding new load variable %s for host %s\n", name, host));
      ep = lAddElemStr(lpp, LR_name, name, LR_Type);
      lSetHost(ep, LR_host, host);
      lSetUlong(ep, LR_global, (u_long32)(strcmp(host, SGE_GLOBAL_NAME) == 0 ? 1 : 0));
      lSetUlong(ep, LR_is_static, sge_is_static_load_value(name));
   }

   lSetString(ep, LR_value, value);

   DPRINTF(("load value %s for host %s: %s\n", name, host, value)); 

   DRETURN(0);
}

