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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cerrno>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_time.h"

#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_answer.h"

#include "schedd_monitor.h"
#include "msg_common.h"

static char schedd_log_file[SGE_PATH_MAX + 1] = "";

void schedd_set_schedd_log_file() {
   DENTER(TOP_LAYER);

   if (!*schedd_log_file) {
      const char *cell_root = bootstrap_get_cell_root();
      snprintf(schedd_log_file, sizeof(schedd_log_file), "%s/%s/%s", cell_root, "common", SCHED_LOG_NAME);
   }

   DRETURN_VOID;
}


int schedd_log(const char *logstr, lList **monitor_alpp, bool monitor_next_run)
{
   DENTER(TOP_LAYER);

   if (monitor_alpp != nullptr) {
      /* add to answer list for verification (-w v) */
      answer_list_add(monitor_alpp, logstr, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
   }

   if (monitor_next_run) {
      /* do logging (-tsm) */
      FILE *fp = fopen(schedd_log_file, "a");
      if (fp == nullptr) {
         DPRINTF("could not open schedd_log_file " SFQ "\n", schedd_log_file);
         DRETURN(-1);
      }

      DSTRING_STATIC(dstr, 64);
      const char *time_str = sge_ctime64(sge_get_gmt64(), &dstr);
      fprintf(fp, "%s|%s\n", time_str, logstr);
      FCLOSE(fp);
   }

   DRETURN(0);
FCLOSE_ERROR:
   DPRINTF(MSG_FILE_ERRORCLOSEINGXY_SS, schedd_log_file, strerror(errno));
   DRETURN(-1);
}


#define NUM_ITEMS_ON_LINE 10

int schedd_log_list(lList **monitor_alpp, bool monitor_next_run, const char *logstr, lList *lp, int nm) {
   DENTER(TOP_LAYER);

   if (monitor_alpp == nullptr && !monitor_next_run) {
      DRETURN(0);
   }

   int fields[] = { 0, 0 };
   fields[0] = nm;

   const char *delis[] = {nullptr, " ", nullptr};
   const lListElem *ep = nullptr;
   lList *lp_part = nullptr;
   for_each_ep(ep, lp) {
      if (lp_part == nullptr) {
         lp_part = lCreateList("partial list", lGetListDescr(lp));
      }
      lAppendElem(lp_part, lCopyElem(ep));
      if (lGetNumberOfElem(lp_part) == NUM_ITEMS_ON_LINE || lNext(ep) == nullptr) {
         char log_string[2048];

         strcpy(log_string, logstr);
         uni_print_list(nullptr,
                        log_string + strlen(log_string),
                        sizeof(log_string) - strlen(log_string) - 1,
                        lp_part,
                        fields, delis, 0);
         schedd_log(log_string, monitor_alpp, monitor_next_run);
         lFreeList(&lp_part);
      }
   }

   lFreeList(&lp_part);

   DRETURN(0);
}

const char *
job_descr(u_long32 jobid) {
   static char descr[20];

   if (jobid) {
      snprintf(descr, sizeof(descr), "Job " sge_u32, jobid);
      return descr;
   }

   return "Job";
}

