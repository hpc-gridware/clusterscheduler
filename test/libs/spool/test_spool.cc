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
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_utility.h"

#include "comm/commlib.h"

#include "gdi/ocs_gdi_client.h"

#include "mir/sge_mirror.h"

#include "spool/sge_spooling.h"
#include "spool/loader/sge_spooling_loader.h"

#include "sig_handlers.h"
#include "msg_clients_common.h"

static lListElem* sge_get_configuration_for_host(const char* aName)
{
   char unique_name[CL_MAXHOSTNAMELEN];
   const lList *cluster_config = *ocs::DataStore::get_master_list(SGE_TYPE_CONFIG);

   DENTER(TOP_LAYER);

   SGE_ASSERT((nullptr != aName));

   /*
    * Due to CR 6319231 IZ 1760:
    *    Try to resolve the hostname
    *    if it is not resolvable then
    *       ignore this and use the given hostname
    */
   int ret = sge_resolve_hostname(aName, unique_name, EH_name);
   if (CL_RETVAL_OK != ret) {
      DPRINTF("%s: error %s resolving host %s\n", __func__, cl_get_error_text(ret), aName);
      strcpy(unique_name, aName);
   }

   lListElem *conf = lCopyElem(lGetElemHost(cluster_config, CONF_name, unique_name));

   DRETURN(conf);
}

static int sge_read_configuration(const lListElem *aSpoolContext, lList *anAnswer)
{
   lListElem *local = nullptr;
   lListElem *global = nullptr;
   int ret = -1;
   const char *cell_root = bootstrap_get_cell_root();
   const char *qualified_hostname = component_get_qualified_hostname();
   u_long32 progid = component_get_component_id();
   lList *cluster_config = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CONFIG);

   DENTER(TOP_LAYER);
   
   spool_read_list(&anAnswer, aSpoolContext, &cluster_config, SGE_TYPE_CONFIG);

   answer_list_output(&anAnswer);

   DPRINTF("qualified_hostname: '%s'\n", qualified_hostname);
   local = sge_get_configuration_for_host(qualified_hostname);
         
   if ((global = sge_get_configuration_for_host(SGE_GLOBAL_NAME)) == nullptr) {
      DRETURN(-1);
   }

   ret = merge_configuration(&anAnswer, progid, cell_root, global, local, nullptr);
   answer_list_output(&anAnswer);

   lFreeElem(&local);
   lFreeElem(&global);
   
   if (0 != ret) {
      DRETURN(-1);
   }
   
   sge_show_conf();         

   DRETURN(0);
}

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
static bool read_spooled_data()
{  
   lList *answer_list = nullptr;
   const lListElem *context;
   lList *master_list = nullptr;
   lList **cluster_configuration = ocs::DataStore::get_master_list_rw(SGE_TYPE_CONFIG);

   DENTER(TOP_LAYER);

   context = spool_get_default_context();

   /* cluster configuration */
   sge_read_configuration(context, answer_list);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master config list\n", lGetNumberOfElem(*cluster_configuration));

   /* cluster configuration */
   {
      lList *schedd_config = nullptr;
      spool_read_list(&answer_list, context, &schedd_config, SGE_TYPE_SCHEDD_CONF);
      if (schedd_config)
         if (sconf_set_config(&schedd_config, &answer_list))
            lFreeList(&schedd_config);
      answer_list_output(&answer_list);
      DPRINTF("read %d entries to master scheduler configuration list\n", lGetNumberOfElem(sconf_get_config_list()));
   }
   /* complexes */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CENTRY);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_CENTRY);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master complex entry list\n", lGetNumberOfElem(master_list));

   /* hosts */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_EXECHOST);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master exechost list\n", lGetNumberOfElem(master_list));

   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_ADMINHOST);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_ADMINHOST);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master admin host list\n", lGetNumberOfElem(master_list));

   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_SUBMITHOST);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_SUBMITHOST);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master submit host list\n", lGetNumberOfElem(master_list));

   /* managers */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_MANAGER);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_MANAGER);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master manager list\n", lGetNumberOfElem(master_list));

   /* host groups */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_HGROUP);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_HGROUP);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master host group list\n", lGetNumberOfElem(master_list));

   /* operators */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_OPERATOR);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_OPERATOR);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master operator list\n", lGetNumberOfElem(master_list));

   /* usersets */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_USERSET);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_USERSET);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master user set list\n", lGetNumberOfElem(master_list));

   /* calendars */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CALENDAR);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_CALENDAR);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master calendar list\n", lGetNumberOfElem(master_list));

   /* queues */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_CQUEUE);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master cluster queue list\n", lGetNumberOfElem(master_list));

   /* pes */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_PE);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_PE);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master parallel environment list\n", lGetNumberOfElem(master_list));

   /* ckpt */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_CKPT);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_CKPT);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master ckpt list\n", lGetNumberOfElem(master_list));

   /* jobs */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_JOB);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master job list\n", lGetNumberOfElem(master_list));

   /* user list */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_USER);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_USER);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master user list\n", lGetNumberOfElem(master_list));

   /* project list */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_PROJECT);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_PROJECT);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master project list\n", lGetNumberOfElem(master_list));

   /* sharetree */
   master_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_SHARETREE);
   spool_read_list(&answer_list, context, &master_list, SGE_TYPE_SHARETREE);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master sharetree list\n", lGetNumberOfElem(master_list));

   DRETURN(true);
}

sge_callback_result spool_event_before([[maybe_unused]] sge_evc_class_t *evc, sge_object_type type,
                                       sge_event_action action, lListElem *event, [[maybe_unused]] void *clientdata)
{
   lList *answer_list = nullptr;
   const lListElem *context, *ep;
   const lList **master_list;
   const lList *new_list;
   int key_nm;
   dstring buffer = DSTRING_INIT;

   DENTER(TOP_LAYER);

   context = spool_get_default_context();
   
   master_list = ocs::DataStore::get_master_list(type);
   key_nm      = object_type_get_key_nm(type);
   new_list    = lGetList(event, ET_new_version);

   if(action == SGE_EMA_LIST) {
      switch(type) {
         case SGE_TYPE_ADMINHOST:      
         case SGE_TYPE_EXECHOST:
         case SGE_TYPE_SUBMITHOST:
         case SGE_TYPE_CONFIG:
         case SGE_TYPE_HGROUP:
            for_each_ep(ep, *master_list) {
               const lListElem *new_ep = lGetElemHost(new_list, key_nm, lGetHost(ep, key_nm));

               if (new_ep == nullptr) {
                  /* object not contained in new list, delete it */
                  spool_delete_object(&answer_list, context, type, lGetHost(ep, key_nm), false);
                  answer_list_output(&answer_list);
               }
            }   

            for_each_ep(ep, new_list) {
               const char *key = lGetHost(ep, key_nm);
               const lListElem *old_ep = lGetElemHost(*master_list, key_nm, key);

               /* check if spooling relevant attributes have changed,
                * if yes, spool the object.
                */
               if (old_ep == nullptr ||
                  spool_compare_objects(&answer_list, context, type, ep, old_ep) != 0)  {
                  spool_write_object(&answer_list, context, ep, key, type, false);
                  answer_list_output(&answer_list);
               }
            }
            break;
         case SGE_TYPE_CALENDAR:
         case SGE_TYPE_CKPT:
         case SGE_TYPE_MANAGER:
         case SGE_TYPE_OPERATOR:
         case SGE_TYPE_PE:
         case SGE_TYPE_PROJECT:
         case SGE_TYPE_CQUEUE:
         case SGE_TYPE_USER:
         case SGE_TYPE_USERSET:
            for_each_ep(ep, *master_list) {
               const lListElem *new_ep = lGetElemStr(new_list, key_nm, lGetString(ep, key_nm));
               if (new_ep == nullptr) {
                  /* object not contained in new list, delete it */
                  spool_delete_object(&answer_list, context, type, lGetString(ep, key_nm), false);
                  answer_list_output(&answer_list);
               }
            }

            for_each_ep(ep, new_list) {
               const char *key = lGetString(ep, key_nm);
               const lListElem *old_ep = lGetElemStr(*master_list, key_nm, key);

               /* check if spooling relevant attributes have changed,
                * if yes, spool the object.
                */
               if(old_ep == nullptr ||
                  spool_compare_objects(&answer_list, context, type, ep, old_ep))  {
                  spool_write_object(&answer_list, context, ep, key, type, false);
                  answer_list_output(&answer_list);
               }
            }
            break;

         case SGE_TYPE_JOB:
            for_each_ep(ep, *master_list) {
               const lListElem *new_ep = lGetElemUlong(new_list, key_nm, lGetUlong(ep, key_nm));
               if (new_ep == nullptr) {
                  const char *job_key;
                  job_key = job_get_key(lGetUlong(ep, key_nm), 0, nullptr, &buffer);
                  /* object not contained in new list, delete it */
                  spool_delete_object(&answer_list, context, type, job_key, false);
                  answer_list_output(&answer_list);
               }
            }

            for_each_ep(ep, new_list) {
               u_long32 key = lGetUlong(ep, key_nm);
               const lListElem *old_ep = lGetElemUlong(*master_list, key_nm, key);

               /* check if spooling relevant attributes have changed,
                * if yes, spool the object.
                */
               if(old_ep == nullptr ||
                  spool_compare_objects(&answer_list, context, type, ep, old_ep))  {
                  const char *job_key;
                  job_key = job_get_key(lGetUlong(ep, key_nm), 0, nullptr, &buffer);
                  spool_write_object(&answer_list, context, ep, job_key, type, false);
                  answer_list_output(&answer_list);
               }
            }
            break;

         case SGE_TYPE_SHARETREE:
            /* two pass algorithm:
             * 1. If we have an old sharetree: delete it
             * 2. If a new sharetree has been sent: write it (spool_event_after)
             * JG: TODO: we have to compare them: delete / write only, when
             *           sharetree no longer exists or has changed
             */
            ep = lFirst(*master_list);
            if(ep != nullptr) {
               /* delete sharetree */
               spool_delete_object(&answer_list, context, type, SHARETREE_FILE, false);
               answer_list_output(&answer_list);
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

                  job_key = job_get_key(job_id, ja_task_id, pe_task_id, &buffer);
                  spool_delete_object(&answer_list, context, type, job_key, false);
                  answer_list_output(&answer_list);
               }
               break;
            case SGE_TYPE_JOB:
               {
                  u_long32 job_id;
                  const char *job_key;

                  job_id = lGetUlong(event, ET_intkey);

                  job_key = job_get_key(job_id, 0, nullptr, &buffer);
                  spool_delete_object(&answer_list, context, type, job_key, false);
                  answer_list_output(&answer_list);
               }
               break;

            default:
               break;
      }
   }

   sge_dstring_free(&buffer);
   DRETURN(SGE_EMA_OK);
}

sge_callback_result
spool_event_after([[maybe_unused]] sge_evc_class_t *evc, sge_object_type type, sge_event_action action,
                  lListElem *event, [[maybe_unused]] void *clientdata)
{
   sge_callback_result ret = SGE_EMA_OK;
   lList *answer_list = nullptr;
   const lListElem *context, *ep;
   const lList **master_list;
   const lList *master_job_list;
   int key_nm;
   const char *key;
   dstring buffer = DSTRING_INIT;

   DENTER(TOP_LAYER);

   context = spool_get_default_context();
   
   master_list = ocs::DataStore::get_master_list(type);
   key_nm      = object_type_get_key_nm(type);
   master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

   switch(action) {
      case SGE_EMA_LIST:
         switch(type) {
            case SGE_TYPE_MANAGER:
            case SGE_TYPE_OPERATOR:
               /* The "classic" spooling functions always write all list entries
                * to the spool file, not individual ones.
                * Therefore we have to call the writing function once after
                * the master list has been updated by the mirroring
                */
               if(strcmp(lGetString(context, SPC_name), "classic spooling") == 0) {
                  ep = lFirst(*master_list);
                  if(ep != nullptr) {
                     spool_write_object(&answer_list, context, ep, nullptr, type, false);
                     answer_list_output(&answer_list);
                  }
               }   
               break;
            case SGE_TYPE_SHARETREE:
               ep = lFirst(*master_list);
               if(ep != nullptr) {
                  /* spool sharetree */
                  spool_write_object(&answer_list, context, ep, SHARETREE_FILE, type, false);
                  answer_list_output(&answer_list);
               }
               break;
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
            case SGE_TYPE_MANAGER:
            case SGE_TYPE_OPERATOR:
            case SGE_TYPE_PE:
            case SGE_TYPE_PROJECT:
            case SGE_TYPE_CQUEUE:
            case SGE_TYPE_USER:
            case SGE_TYPE_USERSET:
            case SGE_TYPE_HGROUP:
               key = lGetString(event, ET_strkey);
               spool_delete_object(&answer_list, context, type, key, false);
               answer_list_output(&answer_list);

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
               if(ep == nullptr) {
                  ERROR("%s element with id " SFQ " not found\n", object_type_get_name(type), key);
                  ret = SGE_EMA_FAILURE;
               }
               if (ret == SGE_EMA_OK) {
                  spool_write_object(&answer_list, context, ep, key, type, false);
                  answer_list_output(&answer_list);
               }
               break;

            case SGE_TYPE_CALENDAR:
            case SGE_TYPE_CKPT:
            case SGE_TYPE_MANAGER:
            case SGE_TYPE_OPERATOR:
            case SGE_TYPE_PE:
            case SGE_TYPE_PROJECT:
            case SGE_TYPE_CQUEUE:
            case SGE_TYPE_USER:
            case SGE_TYPE_USERSET:
            case SGE_TYPE_HGROUP:
               key = lGetString(event, ET_strkey);
               ep = lGetElemStr(*master_list, key_nm, lGetString(event, ET_strkey));
               if(ep == nullptr) {
                  ERROR("%s element with id " SFQ " not found\n", object_type_get_name(type), key);
                  ret = SGE_EMA_FAILURE;
               }
               
               if (ret == SGE_EMA_OK) {
                  spool_write_object(&answer_list, context, ep, key, type, false);
                  answer_list_output(&answer_list);
               }
               break;

            case SGE_TYPE_SCHEDD_CONF:
               ep = lFirst(*master_list);
               if(ep == nullptr) {
                  ERROR("%s element not found\n", object_type_get_name(type));
                  ret = SGE_EMA_FAILURE;
               }
               if (ret == SGE_EMA_OK) {
                  spool_write_object(&answer_list, context, ep, "default", type, false);
                  answer_list_output(&answer_list);
               }
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

                  ep = lGetElemUlong(master_job_list, JB_job_number, job_id);
                  job_key = job_get_key(job_id, ja_task_id, pe_task_id, &buffer);
                  spool_write_object(&answer_list, context, ep, job_key, type, false);
                  answer_list_output(&answer_list);
               }
               break;
            
            default:
               break;
         }
         break;
         
      default:
         break;
   }

   sge_dstring_free(&buffer);

   DRETURN(ret);
}

int main(int argc, char *argv[])
{
   lListElem *spooling_context;
   time_t next_prof_output = 0;
   lList *answer_list = nullptr;
   sge_evc_class_t *evc = nullptr;

   DENTER_MAIN(TOP_LAYER, "test_sge_spooling");

   /* parse commandline parameters */
   if(argc != 4) {
      ERROR("usage: test_sge_spooling <method> <shared lib> <arguments>\n");
      sge_exit(1);
   }

   if (gdi_client_setup_and_enroll(QEVENT, MAIN_THREAD, &answer_list) != AE_OK) {
      answer_list_output(&answer_list);
      sge_exit(1);
   }
   
   sge_setup_sig_handlers(QEVENT);

   // this thread will use the GLOBAL data store
   ocs::DataStore::select_active_ds(ocs::DataStore::Id::GLOBAL);

   if (reresolve_qualified_hostname() != CL_RETVAL_OK) {
      sge_exit(1);
   }

   if (false == sge_gdi2_evc_setup(&evc, EV_ID_SCHEDD, &answer_list, nullptr)) {
      answer_list_output(&answer_list);
      sge_exit(1);
   }

#define defstring(str) #str

   /* initialize spooling */
   spooling_context = spool_create_dynamic_context(&answer_list, argv[1], argv[2], argv[3]); 
   answer_list_output(&answer_list);
   if(spooling_context == nullptr) {
      sge_exit(EXIT_FAILURE);
   }

   spool_set_default_context(spooling_context);

   if(!spool_startup_context(&answer_list, spooling_context, true)) {
      answer_list_output(&answer_list);
      sge_exit(EXIT_FAILURE);
   }
   answer_list_output(&answer_list);
   
   /* read spooled data from disk */
   read_spooled_data();
   
   /* initialize mirroring */
   sge_mirror_initialize(evc, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_ALL, spool_event_before, spool_event_after, nullptr, nullptr, nullptr);
   prof_start(SGE_PROF_ALL, nullptr);

   while(!shut_me_down) {
      time_t now;
      
      sge_mirror_process_events(evc);

      now = time(nullptr);
      if (now > next_prof_output) {
         prof_output_info(SGE_PROF_ALL, false, "test_sge_info:\n");
/*          INFO("\n%s", prof_get_info_string(SGE_PROF_ALL, false, nullptr)); */
         next_prof_output = now + 60;
      }
   }

   sge_mirror_shutdown(evc);

   spool_shutdown_context(&answer_list, spooling_context);
   answer_list_output(&answer_list);

   DRETURN(EXIT_SUCCESS);
}
