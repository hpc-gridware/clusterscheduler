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
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "sge_unistd.h"
#include "sge_all_listsL.h"
#include "usage.h"
#include "sig_handlers.h"
#include "commlib.h"
#include "sge_prog.h"
#include "sgermon.h"
#include "sge_log.h"

#include "sge_profiling.h"
#include "sge_host.h"
#include "sge_calendar.h"
#include "sge_ckpt.h"
#include "sge_complex.h"
#include "sge_conf.h"
#include "sge_job.h"
#include "sge_manop.h"
#include "sge_sharetree.h"
#include "sge_pe.h"
#include "sge_queue.h"
#include "sge_schedd_conf.h"
#include "sge_userprj.h"
#include "sge_userset.h"

#ifndef __SGE_NO_USERMAPPING__
#include "sge_usermap.h"
#endif

#include "sge_hostgroup.h"


#include "msg_clients_common.h"

#include "sge_mirror.h"
#include "sge_spooling_classic.h"
#include "sge_event.h"

/* JG: TODO: This should be a public interface in libspool
 *           that can be used in all modules unspooling data.
 *           Therefore it will be necessary to extract some
 *           special handling and processing still contained
 *           in the reading functions (classic spooling) into
 *           other interfaces, e.g. use callbacks).
 *            
 *           Instead of hardcoding each list, we could loop over
 *           the sge_object_type enum. Problem is currently that
 *           a certain order of unspooling is required.
 *           This could be eliminated by splitting the read list 
 *           functions (from classic spooling) into reading and 
 *           post processing.
 *           
 *           If we do not need to spool/unspool all lists in a certain
 *           spooling client, we could even require that subscription
 *           has been done before calling this functions and call
 *           a function sge_mirror_is_subscribed function to check if we have
 *           to unspool a certain list or not.
 */
static bool read_spooled_data(void)
{  
   lListElem *context;

   context = spool_get_default_context();

   /* cluster configuration */
   spool_read_list(context, &Master_Config_List, SGE_TYPE_CONFIG);
   DPRINTF(("read %d entries to Master_Config_List\n", lGetNumberOfElem(Master_Config_List)));

   /* complexes */
   spool_read_list(context, &Master_Complex_List, SGE_TYPE_COMPLEX);
   DPRINTF(("read %d entries to Master_Complex_List\n", lGetNumberOfElem(Master_Complex_List)));

   /* hosts */
   spool_read_list(context, &Master_Exechost_List, SGE_TYPE_EXECHOST);
   DPRINTF(("read %d entries to Master_Exechost_List\n", lGetNumberOfElem(Master_Exechost_List)));
   spool_read_list(context, &Master_Adminhost_List, SGE_TYPE_ADMINHOST);
   DPRINTF(("read %d entries to Master_Adminhost_List\n", lGetNumberOfElem(Master_Adminhost_List)));
   spool_read_list(context, &Master_Submithost_List, SGE_TYPE_SUBMITHOST);
   DPRINTF(("read %d entries to Master_Submithost_List\n", lGetNumberOfElem(Master_Submithost_List)));

   /* managers */
   spool_read_list(context, &Master_Manager_List, SGE_TYPE_MANAGER);
   DPRINTF(("read %d entries to Master_Manager_List\n", lGetNumberOfElem(Master_Manager_List)));

   /* host groups */
   spool_read_list(context, &Master_Host_Group_List, SGE_TYPE_HOSTGROUP);
   DPRINTF(("read %d entries to Master_Hostgroup_List\n", lGetNumberOfElem(Master_Host_Group_List)));

   /* operators */
   spool_read_list(context, &Master_Operator_List, SGE_TYPE_OPERATOR);
   DPRINTF(("read %d entries to Master_Operator_List\n", lGetNumberOfElem(Master_Operator_List)));

   /* usersets */
   spool_read_list(context, &Master_Userset_List, SGE_TYPE_USERSET);
   DPRINTF(("read %d entries to Master_Userset_List\n", lGetNumberOfElem(Master_Userset_List)));

   /* calendars */
   spool_read_list(context, &Master_Calendar_List, SGE_TYPE_CALENDAR);
   DPRINTF(("read %d entries to Master_Calendar_List\n", lGetNumberOfElem(Master_Calendar_List)));

#ifndef __SGE_NO_USERMAPPING__
   /* user mapping */
   spool_read_list(context, &Master_Usermapping_Entry_List, SGE_TYPE_USERMAPPING);
   DPRINTF(("read %d entries to Master_Usermapping_List\n", lGetNumberOfElem(Master_Usermapping_Entry_List)));
#endif

   /* queues */
   spool_read_list(context, &Master_Queue_List, SGE_TYPE_QUEUE);
   DPRINTF(("read %d entries to Master_Queue_List\n", lGetNumberOfElem(Master_Queue_List)));

   /* pes */
   spool_read_list(context, &Master_Pe_List, SGE_TYPE_PE);
   DPRINTF(("read %d entries to Master_Pe_List\n", lGetNumberOfElem(Master_Pe_List)));

   /* ckpt */
   spool_read_list(context, &Master_Ckpt_List, SGE_TYPE_CKPT);
   DPRINTF(("read %d entries to Master_Ckpt_List\n", lGetNumberOfElem(Master_Ckpt_List)));

   /* jobs */
   spool_read_list(context, &Master_Job_List, SGE_TYPE_JOB);
   DPRINTF(("read %d entries to Master_Job_List\n", lGetNumberOfElem(Master_Job_List)));

   /* user list */
   spool_read_list(context, &Master_User_List, SGE_TYPE_USER);
   DPRINTF(("read %d entries to Master_User_List\n", lGetNumberOfElem(Master_User_List)));

   /* project list */
   spool_read_list(context, &Master_Project_List, SGE_TYPE_PROJECT);
   DPRINTF(("read %d entries to Master_Project_List\n", lGetNumberOfElem(Master_Project_List)));

   /* sharetree */
   spool_read_list(context, &Master_Sharetree_List, SGE_TYPE_SHARETREE);
   DPRINTF(("read %d entries to Master_Sharetree_List\n", lGetNumberOfElem(Master_Sharetree_List)));

   return true;
}

bool spool_event_before(sge_object_type type, sge_event_action action, 
                       lListElem *event, void *clientdata)
{
   lListElem *context, *ep;
   lList **master_list, *new_list;
   int key_nm;

   DENTER(TOP_LAYER, "spool_event_before");

   context = spool_get_default_context();
   
   master_list = object_type_get_master_list(type);
   key_nm      = object_type_get_key_nm(type);
   new_list    = lGetList(event, ET_new_version);

   if(action == SGE_EMA_LIST) {
      switch(type) {
         case SGE_TYPE_ADMINHOST:      
         case SGE_TYPE_EXECHOST:
         case SGE_TYPE_SUBMITHOST:
         case SGE_TYPE_CONFIG:
            for_each(ep, *master_list) {
               lListElem *new_ep;

               new_ep = lGetElemHost(new_list, key_nm, lGetHost(ep, key_nm));
               if(new_ep == NULL) {
                  /* object not contained in new list, delete it */
                  spool_delete_object(context, type, lGetHost(ep, key_nm));
               }
            }   

            for_each(ep, new_list) {
               lListElem *old_ep;
               const char *key = lGetHost(ep, key_nm);

               old_ep = lGetElemHost(*master_list, key_nm, key);

               /* check if spooling relevant attributes have changed,
                * if yes, spool the object.
                */
               if(old_ep == NULL || 
                  spool_compare_objects(context, type, ep, old_ep) != 0)  {
                  spool_write_object(context, ep, key, type);
               }
            }
            break;
         case SGE_TYPE_CALENDAR:
         case SGE_TYPE_CKPT:
         case SGE_TYPE_COMPLEX:
         case SGE_TYPE_MANAGER:
         case SGE_TYPE_OPERATOR:
         case SGE_TYPE_PE:
         case SGE_TYPE_PROJECT:
         case SGE_TYPE_QUEUE:
         case SGE_TYPE_USER:
         case SGE_TYPE_USERSET:
#ifndef __SGE_NO_USERMAPPING__
         case SGE_TYPE_USERMAPPING:
#endif
         case SGE_TYPE_HOSTGROUP:
            for_each(ep, *master_list) {
               lListElem *new_ep;

               new_ep = lGetElemStr(new_list, key_nm, lGetString(ep, key_nm));
               if(new_ep == NULL) {
                  /* object not contained in new list, delete it */
                  spool_delete_object(context, type, lGetString(ep, key_nm));
               }
            }

            for_each(ep, new_list) {
               lListElem *old_ep;
               const char *key = lGetString(ep, key_nm);

               old_ep = lGetElemStr(*master_list, key_nm, key);

               /* check if spooling relevant attributes have changed,
                * if yes, spool the object.
                */
               if(old_ep == NULL || 
                  spool_compare_objects(context, type, ep, old_ep) != 0)  {
                  spool_write_object(context, ep, key, type);
               }
            }
            break;
         default:
            break;
      }
   }

   if(action == SGE_EMA_DEL) {
      switch(type) {
            case SGE_TYPE_JATASK:
            case SGE_TYPE_PETASK:
               {
                  u_long32 job_id, ja_task_id;
                  const char *pe_task_id;
                  const char *job_key;

                  job_id = lGetUlong(event, ET_intkey);
                  ja_task_id = lGetUlong(event, ET_intkey2);
                  pe_task_id = lGetString(event, ET_strkey);

                  job_key = job_get_key(job_id, ja_task_id, pe_task_id);
                  spool_delete_object(context, type, job_key);
               }
               break;
            case SGE_TYPE_JOB:
               {
                  u_long32 job_id;
                  const char *job_key;

                  job_id = lGetUlong(event, ET_intkey);

                  job_key = job_get_key(job_id, 0, NULL);
                  spool_delete_object(context, type, job_key);
               }
               break;

            default:
               break;
      }
   }
   DEXIT;
   return true;
}

bool spool_event_after(sge_object_type type, sge_event_action action, 
                      lListElem *event, void *clientdata)
{
   lListElem *context, *ep;
   lList **master_list;
   int key_nm;
   const char *key;

   DENTER(TOP_LAYER, "spool_event_after");

   context = spool_get_default_context();
   
   master_list = object_type_get_master_list(type);
   key_nm      = object_type_get_key_nm(type);

   switch(action) {
      case SGE_EMA_LIST:
         switch(type) {
            case SGE_TYPE_SHARETREE:
               ep = lFirst(*master_list);
               if(ep == NULL) {
                  /* delete sharetree */
                  spool_delete_object(context, type, SHARETREE_FILE);
               } else {
                  /* spool sharetree */
                  spool_write_object(context, ep, NULL, type);
               }
               break;
            case SGE_TYPE_MANAGER:
            case SGE_TYPE_OPERATOR:
               /* The "classic" spooling functions always write all list entries
                * to the spool file, not individual ones.
                * Therefore we have to call the writing function once after
                * the master list has been updated by the mirroring
                */
               if(strcmp(lGetString(context, SPC_name), "classic spooling") == 0) {
                  ep = lFirst(*master_list);
                  if(ep != NULL) {
                     spool_write_object(context, ep, NULL, type);
                  }
               }   
            default:
               break;
         }
         break;
   
      case SGE_EMA_DEL:
         switch(type) {
            case SGE_TYPE_ADMINHOST:
            case SGE_TYPE_EXECHOST:
            case SGE_TYPE_SUBMITHOST:
            case SGE_TYPE_CONFIG:
            case SGE_TYPE_CALENDAR:
            case SGE_TYPE_CKPT:
            case SGE_TYPE_COMPLEX:
            case SGE_TYPE_MANAGER:
            case SGE_TYPE_OPERATOR:
            case SGE_TYPE_PE:
            case SGE_TYPE_PROJECT:
            case SGE_TYPE_QUEUE:
            case SGE_TYPE_USER:
            case SGE_TYPE_USERSET:
#ifndef __SGE_NO_USERMAPPING__
            case SGE_TYPE_USERMAPPING:
#endif   
            case SGE_TYPE_HOSTGROUP:
               key = lGetString(event, ET_strkey);
               spool_delete_object(context, type, key);

            default:
               break;
         }
         break;

      case SGE_EMA_ADD:
      case SGE_EMA_MOD:
         switch(type) {
            case SGE_TYPE_ADMINHOST:
            case SGE_TYPE_EXECHOST:
            case SGE_TYPE_SUBMITHOST:
            case SGE_TYPE_CONFIG:
               key = lGetString(event, ET_strkey);
               ep = lGetElemHost(*master_list, key_nm, lGetString(event, ET_strkey));
               if(ep == NULL) {
                  ERROR((SGE_EVENT, "%s element with id "SFQ" not found\n",
                         object_type_get_name(type), key));
                  DEXIT;
                  return false;
               }

               spool_write_object(context, ep, key, type);
               break;

            case SGE_TYPE_CALENDAR:
            case SGE_TYPE_CKPT:
            case SGE_TYPE_COMPLEX:
            case SGE_TYPE_MANAGER:
            case SGE_TYPE_OPERATOR:
            case SGE_TYPE_PE:
            case SGE_TYPE_PROJECT:
            case SGE_TYPE_QUEUE:
            case SGE_TYPE_USER:
            case SGE_TYPE_USERSET:
#ifndef __SGE_NO_USERMAPPING__
            case SGE_TYPE_USERMAPPING:
#endif
            case SGE_TYPE_HOSTGROUP:
               key = lGetString(event, ET_strkey);
               ep = lGetElemStr(*master_list, key_nm, lGetString(event, ET_strkey));
               if(ep == NULL) {
                  ERROR((SGE_EVENT, "%s element with id "SFQ" not found\n",
                         object_type_get_name(type), key));
                  DEXIT;
                  return false;
               }

               spool_write_object(context, ep, key, type);
               break;

            case SGE_TYPE_SCHEDD_CONF:
               ep = lFirst(*master_list);
               if(ep == NULL) {
                  ERROR((SGE_EVENT, "%s element not found\n",
                         object_type_get_name(type)));
                  DEXIT;
                  return false;
               }
               spool_write_object(context, ep, NULL, type);
               break;
            case SGE_TYPE_JATASK:
            case SGE_TYPE_PETASK:
            case SGE_TYPE_JOB:
               {
                  u_long32 job_id, ja_task_id;
                  const char *pe_task_id;
                  const char *job_key;

                  job_id = lGetUlong(event, ET_intkey);
                  ja_task_id = lGetUlong(event, ET_intkey2);
                  pe_task_id = lGetString(event, ET_strkey);

                  ep = lGetElemUlong(Master_Job_List, JB_job_number, job_id);
                  job_key = job_get_key(job_id, ja_task_id, pe_task_id);
                  spool_write_object(context, ep, job_key, type);
               }
               break;
            
            default:
               break;
         }
         break;
         
      default:
         break;
   }

   DEXIT;
   return true;
}

int main(int argc, char *argv[])
{
   int cl_err = 0;
   char *cwd;
   dstring common_dir = DSTRING_INIT;
   dstring spool_dir  = DSTRING_INIT;
   lListElem *spooling_context;

   DENTER_MAIN(TOP_LAYER, "test_sge_mirror");

   /* parse commandline parameters */
   if(argc != 3) {
      ERROR((SGE_EVENT, "usage: test_sge_spooling <shared lib> <arguments>\n"));
      SGE_EXIT(1);
   }

   sge_gdi_param(SET_MEWHO, QEVENT, NULL);
   if ((cl_err = sge_gdi_setup(prognames[QEVENT]))) {
      ERROR((SGE_EVENT, MSG_GDI_SGE_SETUP_FAILED_S, cl_errstr(cl_err)));
      SGE_EXIT(1);
   }

   sge_setup_sig_handlers(QEVENT);

   if (reresolve_me_qualified_hostname() != CL_OK) {
      SGE_EXIT(1);
   }   

#define defstring(str) #str

   /* initialize spooling */
   spooling_context = spool_create_dynamic_context(SPOOL_NAME(SPOOLING_METHOD), 
                                                   argv[1], argv[2]); 
   if(spooling_context == NULL) {
      SGE_EXIT(EXIT_FAILURE);
   }

   spool_set_default_context(spooling_context);

   if(!spool_startup_context(spooling_context)) {
      SGE_EXIT(EXIT_FAILURE);
   }
   
   /* read spooled data from disk */
   read_spooled_data();
   
   /* initialize mirroring */
   sge_mirror_initialize(EV_ID_ANY, "test_sge_mirror");
   sge_mirror_subscribe(SGE_TYPE_ALL, spool_event_before, spool_event_after, NULL);
   profiling_start();

   while(!shut_me_down) {
      sge_mirror_process_events();
   }

   sge_mirror_shutdown();

   DEXIT;
   return EXIT_SUCCESS;
}
