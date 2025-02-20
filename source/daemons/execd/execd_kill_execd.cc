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

#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"

#include "gdi/ocs_gdi_ClientServerBase.h"

#include "comm/commlib.h"

#include "dispatcher.h"
#include "sge_load_sensor.h"
#include "execd_kill_execd.h"
#include "execd_signal_queue.h"
#include "symbols.h"
#include "msg_execd.h"

extern int shut_me_down;

int do_kill_execd(ocs::gdi::ClientServerBase::struct_msg_t *aMsg)
{
   const lListElem *jep;
   lListElem *jatep;
   u_long32 kill_jobs;
   u_long32 sge_signal;
   
   DENTER(TOP_LAYER);

   /* real shut down is done in the execd_ck_to_do function */

   unpackint(&(aMsg->buf), &kill_jobs);

   DPRINTF("===>KILL EXECD%s\n", kill_jobs?" and jobs":"");
   if (kill_jobs) {
      for_each_ep(jep, *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB)) {
         for_each_rw (jatep, lGetList(jep, JB_ja_tasks)) {
            if (lGetUlong(jep, JB_checkpoint_attr) & CHECKPOINT_AT_SHUTDOWN) {
               WARNING(MSG_JOB_INITCKPTSHUTDOWN_U, lGetUlong(jep, JB_job_number));
               sge_signal = SGE_MIGRATE;
            } else {
               WARNING(MSG_JOB_KILLSHUTDOWN_U, lGetUlong(jep, JB_job_number));
               sge_signal = SGE_SIGKILL;
            }
            sge_execd_deliver_signal(sge_signal, jep, jatep);
         }
      }
   }

   shut_me_down = 1;

   DRETURN(0);
}

