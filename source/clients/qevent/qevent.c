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

#include <stdio.h>
#include <stdlib.h>

#include "sge_unistd.h"
#include "sge_gdi_intern.h"
#include "sge_all_listsL.h"
#include "usage.h"
#include "sig_handlers.h"
#include "commlib.h"
#include "sge_prog.h"
#include "sgermon.h"
#include "sge_log.h"

#include "msg_clients_common.h"

#include "sge_mirror.h"
#include "sge_event.h"
#include "sge_c_event.h"


u_long Global_jobs_running = 0;

int print_event(sge_event_type type, sge_event_action action, 
                lListElem *event, void *clientdata)
{
   DPRINTF(("%s\n", event_text(event)));
   /* create a callback error to test error handling */
   if(type == SGE_EMT_GLOBAL_CONFIG) {
      return FALSE;
   }
   
   return TRUE;
}

int print_jatask_event(sge_event_type type, sge_event_action action, 
                lListElem *event, void *clientdata)
{
   int pos;

   DPRINTF(("%s\n", event_text(event)));
/*    fprintf(stdout,"%s\n",event_text(event)); */
   if ((pos=lGetPosViaElem(event, ET_type))>=0) {
      u_long32 type = lGetUlong(event, ET_type);
      if (type == sgeE_JATASK_MOD) { 
         lList *jat = lGetList(event,ET_new_version);
         u_long job_id  = lGetUlong(event, ET_intkey);
         u_long task_id = lGetUlong(event, ET_intkey2);
         lListElem *ep = lFirst(jat);
         u_long job_status = lGetUlong(ep, JAT_status);
         int task_running = (job_status==JRUNNING || job_status==JTRANSFERING);
         if (task_running) {
            fprintf(stdout,"START (%ld.%ld)\n", job_id ,task_id);
            fflush(stdout);  
            Global_jobs_running++;
         }
/*         lWriteElemTo(event, stdout); 
         fflush(stdout); */
      }
      if (type == sgeE_JOB_FINAL_USAGE) { 
         /* lList *jat = lGetList(event,ET_new_version); */
         u_long job_id = lGetUlong(event, ET_intkey);
         u_long task_id = lGetUlong(event, ET_intkey2);
         /* lWriteElemTo(event, stdout); */
         fprintf(stdout,"FINISH (%ld.%ld)\n", job_id, task_id);
         Global_jobs_running--;
         fflush(stdout);  
      }

   }
   /* create a callback error to test error handling */
   if(type == SGE_EMT_GLOBAL_CONFIG) {
      return FALSE;
   }
   
   return TRUE;
}


int main(int argc, char *argv[])
{
   int cl_err = 0;

   DENTER_MAIN(TOP_LAYER, "test_sge_mirror");

   sge_gdi_param(SET_MEWHO, QEVENT, NULL);
   if ((cl_err = sge_gdi_setup(prognames[QEVENT]))) {
      ERROR((SGE_EVENT, MSG_GDI_SGE_SETUP_FAILED_S, cl_errstr(cl_err)));
      SGE_EXIT(1);
   }

   sge_setup_sig_handlers(QEVENT);

   if (reresolve_me_qualified_hostname() != CL_OK) {
      SGE_EXIT(1);
   }   

   sge_mirror_initialize(EV_ID_ANY, "test_sge_mirror");
   sge_mirror_subscribe(SGE_EMT_JOB, print_jatask_event, NULL, NULL);
   sge_mirror_subscribe(SGE_EMT_JATASK ,print_jatask_event, NULL, NULL);
   
   ec_set_flush(sgeE_JATASK_MOD,0);
   ec_set_flush(sgeE_JOB_FINAL_USAGE,0);
/*   sge_mirror_subscribe(SGE_EMT_ALL, print_event, NULL, NULL); */
   
   while(!shut_me_down) {
      sge_mirror_process_events();
      fprintf(stdout,"Running: %ld\n",Global_jobs_running);
      fflush(stdout);  
   }

   sge_mirror_shutdown();

   DEXIT;
   return EXIT_SUCCESS;
}
