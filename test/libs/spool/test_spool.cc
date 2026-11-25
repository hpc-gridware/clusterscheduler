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
 * @brief Unit tests for the spooling framework
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

#include "gdi/ocs_gdi_Client.h"

#include "mir/sge_mirror.h"

#include "spool/sge_spooling.h"
#include "spool/loader/sge_spooling_loader.h"

#include "sig_handlers.h"

/**
 * @brief A copy of the configuration of a host
 * @param aName the host name, resolved if possible
 * @return the copy, empty if the host has no configuration
 */
static ocs::tom::ElementPtr<ocs::tom::CONF> sge_get_configuration_for_host(const char *aName) {
   DENTER(TOP_LAYER);

   char unique_name[CL_MAXHOSTNAMELEN];
   const auto cluster_config = ocs::DataStore::master_list<SGE_TYPE_CONFIG>();

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
      if (unique_name[0] == '\0') {
         // over-long: sge_resolve_hostname() left it empty, no configuration has such a name
         DRETURN(nullptr);
      }
      sge_strlcpy(unique_name, aName, sizeof(unique_name));
   }

   auto conf = cluster_config.find_first<CONF_name>(ocs::tom::Host(unique_name)).clone();

   DRETURN(conf);
}

/**
 * @brief Read the cluster configuration from the spool and merge the global and the local one
 * @param aSpoolContext the spooling context
 * @param anAnswer the answer list
 * @return 0 on success, -1 otherwise
 */
static int sge_read_configuration(const ocs::tom::ConstElement<ocs::tom::SPC> aSpoolContext,
                                  ocs::tom::ListPtr<ocs::tom::AN> *anAnswer) {
   DENTER(TOP_LAYER);

   int ret = -1;
   const char *cell_root = bootstrap_get_cell_root();
   const char *qualified_hostname = component_get_qualified_hostname();
   uint32_t progid = component_get_component_id();
   // read into the data store slot itself: a list created for an empty slot belongs to the data store
   spool_read_list(anAnswer, aSpoolContext, ocs::DataStore::master_list_slot(SGE_TYPE_CONFIG), SGE_TYPE_CONFIG);

   answer_list_output(anAnswer);

   DPRINTF("qualified_hostname: '%s'\n", qualified_hostname);
   auto local = sge_get_configuration_for_host(qualified_hostname);

   auto global = sge_get_configuration_for_host(SGE_GLOBAL_NAME);
   if (global == nullptr) {
      DRETURN(-1);
   }

   ret = merge_configuration(anAnswer, progid, cell_root, *global, *local).first;
   answer_list_output(anAnswer);

   local.reset();
   global.reset();
   
   if (0 != ret) {
      DRETURN(-1);
   }
   
   sge_show_conf();         

   DRETURN(0);
}

/**
 * @brief Read one master list from the spool into its data store slot
 *
 * The list is read into the slot itself, not into a copy of the slot's pointer: a list the read creates for an
 * empty slot then belongs to the data store instead of a local.
 *
 * @tparam V the object type of the list
 * @param answer_list receives the messages, which are written out
 * @param context the spooling context
 * @param name what the list is called in the trace
 */
template <sge_object_type V>
static void
read_master_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::ConstElement<ocs::tom::SPC> context,
                 const char *name) {
   DENTER(TOP_LAYER);
   spool_read_list(answer_list, context, ocs::DataStore::master_list_slot(V), V);
   answer_list_output(answer_list);
   DPRINTF("read %d entries to master %s list\n", static_cast<int>(ocs::DataStore::master_list<V>().size()), name);
   DRETURN_VOID;
}

/**
 * @brief Read every object list of the cluster from the spooling context
 *
 * Should become a public interface of libspool usable by every module that unspools data; that needs the
 * special handling still contained in the classic spooling read functions moved out (e.g. into callbacks).
 * Instead of hardcoding each list it could loop over the sge_object_type enum, but a certain order of
 * unspooling is required today; splitting the read functions into reading and post processing would remove
 * that. A client that does not need every list could require the subscription first and check it with
 * sge_mirror_is_subscribed().
 *
 * @return true when every list was read
 */
static bool read_spooled_data() {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

   const auto context = spool_get_default_context();

   /* cluster configuration */
   sge_read_configuration(context, &answer_list);
   answer_list_output(&answer_list);
   DPRINTF("read %d entries to master config list\n",
           static_cast<int>(ocs::DataStore::master_list<SGE_TYPE_CONFIG>().size()));

   /* cluster configuration */
   {
      ocs::tom::ListData *schedd_config = nullptr;
      spool_read_list(&answer_list, context, &schedd_config, SGE_TYPE_SCHEDD_CONF);
      if (schedd_config) {
         // the handle takes the spooled list over; a list the call refused is freed with it
         ocs::tom::ListPtr<ocs::tom::SC> config(schedd_config);
         if (sconf_set_config(config, &answer_list)) {
            config.reset();
         }
      }
      answer_list_output(&answer_list);
      DPRINTF("read %d entries to master scheduler configuration list\n",
              static_cast<int>(sconf_get_config_list().get().size()));
   }
   /* complexes */
   read_master_list<SGE_TYPE_CENTRY>(&answer_list, context, "complex entry");

   /* hosts */
   read_master_list<SGE_TYPE_EXECHOST>(&answer_list, context, "exechost");

   /* CS-2394: managers/operators are no longer spooled as own lists - they are
    * members of the reserved "manager"/"operator" usersets, covered below. */

   /* host groups */
   read_master_list<SGE_TYPE_HGROUP>(&answer_list, context, "host group");

   /* usersets */
   read_master_list<SGE_TYPE_USERSET>(&answer_list, context, "user set");

   /* calendars */
   read_master_list<SGE_TYPE_CALENDAR>(&answer_list, context, "calendar");

   /* queues */
   read_master_list<SGE_TYPE_CQUEUE>(&answer_list, context, "cluster queue");

   /* pes */
   read_master_list<SGE_TYPE_PE>(&answer_list, context, "parallel environment");

   /* ckpt */
   read_master_list<SGE_TYPE_CKPT>(&answer_list, context, "ckpt");

   /* jobs */
   read_master_list<SGE_TYPE_JOB>(&answer_list, context, "job");

   /* user list */
   read_master_list<SGE_TYPE_USER>(&answer_list, context, "user");

   /* project list */
   read_master_list<SGE_TYPE_PROJECT>(&answer_list, context, "project");

   /* sharetree */
   read_master_list<SGE_TYPE_SHARETREE>(&answer_list, context, "sharetree");

   DRETURN(true);
}

/** @brief Spool event before
 *
 * @param evc see the description above
 * @param type see the description above
 * @param action see the description above
 * @param event see the description above
 * @param clientdata see the description above
  * @return `SGE_EMA_OK` to let the mirror carry on
 */
sge_callback_result spool_event_before([[maybe_unused]] sge_evc_class_t *evc, const sge_object_type type,
                                       const sge_event_action action,
                                       const ocs::tom::ConstElement<ocs::tom::ET> event,
                                       [[maybe_unused]] void *clientdata) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   ocs::tom::ConstRawElement ep;
   dstring buffer = DSTRING_INIT;

   const auto context = spool_get_default_context();

   const ocs::tom::ConstRawList master_list = ocs::DataStore::master_list_raw(type);
   const int key_nm = object_type_get_key_nm(type);
   const ocs::tom::ConstRawList new_list = event.get<ET_new_version>();

   if(action == SGE_EMA_LIST) {
      switch(type) {
         case SGE_TYPE_EXECHOST:
         case SGE_TYPE_CONFIG:
         case SGE_TYPE_HGROUP:
            for (const ocs::tom::ConstRawElement old : master_list) {
               const auto new_ep = new_list.find_first(key_nm, old.get<ocs::tom::Host>(key_nm));

               if (new_ep == nullptr) {
                  /* object not contained in new list, delete it */
                  spool_delete_object(&answer_list, context, type, old.get<ocs::tom::Host>(key_nm).c_str());
                  answer_list_output(&answer_list);
               }
            }

            for (const ocs::tom::ConstRawElement elem : new_list) {
               ep = elem;
               const ocs::tom::Host key = ep.get<ocs::tom::Host>(key_nm);
               const auto old_ep = master_list.find_first(key_nm, key);

               /* check if spooling relevant attributes have changed,
                * if yes, spool the object.
                */
               if (old_ep == nullptr ||
                  spool_compare_objects(&answer_list, context, type, ep, old_ep) != 0)  {
                  spool_write_object(&answer_list, context, ep, key.c_str(), type);
                  answer_list_output(&answer_list);
               }
            }
            break;
         case SGE_TYPE_CALENDAR:
         case SGE_TYPE_CKPT:
         case SGE_TYPE_PE:
         case SGE_TYPE_PROJECT:
         case SGE_TYPE_CQUEUE:
         case SGE_TYPE_USER:
         case SGE_TYPE_USERSET:
            for (const ocs::tom::ConstRawElement old : master_list) {
               const auto new_ep = new_list.find_first(key_nm, old.get<const char *>(key_nm));
               if (new_ep == nullptr) {
                  /* object not contained in new list, delete it */
                  spool_delete_object(&answer_list, context, type, old.get<const char *>(key_nm));
                  answer_list_output(&answer_list);
               }
            }

            for (const ocs::tom::ConstRawElement elem : new_list) {
               ep = elem;
               const char *key = ep.get<const char *>(key_nm);
               const auto old_ep = master_list.find_first(key_nm, key);

               /* check if spooling relevant attributes have changed,
                * if yes, spool the object.
                */
               if(old_ep == nullptr ||
                  spool_compare_objects(&answer_list, context, type, ep, old_ep))  {
                  spool_write_object(&answer_list, context, ep, key, type);
                  answer_list_output(&answer_list);
               }
            }
            break;

         case SGE_TYPE_JOB:
            for (const ocs::tom::ConstRawElement old : master_list) {
               const auto new_ep = new_list.find_first(key_nm, old.get<uint32_t>(key_nm));
               if (new_ep == nullptr) {
                  const char *job_key;
                  job_key = job_get_key(old.get<uint32_t>(key_nm), 0, nullptr, &buffer);
                  /* object not contained in new list, delete it */
                  spool_delete_object(&answer_list, context, type, job_key);
                  answer_list_output(&answer_list);
               }
            }

            for (const ocs::tom::ConstRawElement elem : new_list) {
               ep = elem;
               uint32_t key = ep.get<uint32_t>(key_nm);
               const auto old_ep = master_list.find_first(key_nm, key);

               /* check if spooling relevant attributes have changed,
                * if yes, spool the object.
                */
               if(old_ep == nullptr ||
                  spool_compare_objects(&answer_list, context, type, ep, old_ep))  {
                  const char *job_key;
                  job_key = job_get_key(ep.get<uint32_t>(key_nm), 0, nullptr, &buffer);
                  spool_write_object(&answer_list, context, ep, job_key, type);
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
            ep = master_list.front();
            if(ep != nullptr) {
               /* delete sharetree */
               spool_delete_object(&answer_list, context, type, SHARETREE_FILE);
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
                  uint32_t job_id, ja_task_id;
                  const char *pe_task_id;
                  const char *job_key;

                  job_id = event.get<ET_intkey>();
                  ja_task_id = event.get<ET_intkey2>();
                  pe_task_id = event.get<ET_strkey>();

                  job_key = job_get_key(job_id, ja_task_id, pe_task_id, &buffer);
                  spool_delete_object(&answer_list, context, type, job_key);
                  answer_list_output(&answer_list);
               }
               break;
            case SGE_TYPE_JOB:
               {
                  uint32_t job_id;
                  const char *job_key;

                  job_id = event.get<ET_intkey>();

                  job_key = job_get_key(job_id, 0, nullptr, &buffer);
                  spool_delete_object(&answer_list, context, type, job_key);
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

/** @brief Spool event after
 *
 * @param evc see the description above
 * @param type see the description above
 * @param action see the description above
 * @param event see the description above
 * @param clientdata see the description above
  * @return `SGE_EMA_OK` to let the mirror carry on
 */
sge_callback_result
spool_event_after([[maybe_unused]] sge_evc_class_t *evc, const sge_object_type type, const sge_event_action action,
                  const ocs::tom::ConstElement<ocs::tom::ET> event, [[maybe_unused]] void *clientdata) {
   DENTER(TOP_LAYER);

   sge_callback_result ret = SGE_EMA_OK;
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   ocs::tom::ConstRawElement ep;
   const char *key;
   dstring buffer = DSTRING_INIT;

   const auto context = spool_get_default_context();

   const ocs::tom::ConstRawList master_list = ocs::DataStore::master_list_raw(type);
   const int key_nm = object_type_get_key_nm(type);
   const auto master_job_list = ocs::DataStore::master_list<SGE_TYPE_JOB>();

   switch(action) {
      case SGE_EMA_LIST:
         switch(type) {
            case SGE_TYPE_SHARETREE:
               ep = master_list.front();
               if(ep != nullptr) {
                  /* spool sharetree */
                  spool_write_object(&answer_list, context, ep, SHARETREE_FILE, type);
                  answer_list_output(&answer_list);
               }
               break;
            default:
               break;
         }
         break;
   
      case SGE_EMA_DEL:
         switch(type) {
            case SGE_TYPE_EXECHOST:
            case SGE_TYPE_CONFIG:
            case SGE_TYPE_CALENDAR:
            case SGE_TYPE_CKPT:
            case SGE_TYPE_PE:
            case SGE_TYPE_PROJECT:
            case SGE_TYPE_CQUEUE:
            case SGE_TYPE_USER:
            case SGE_TYPE_USERSET:
            case SGE_TYPE_HGROUP:
               key = event.get<ET_strkey>();
               spool_delete_object(&answer_list, context, type, key);
               answer_list_output(&answer_list);

            default:
               break;
         }
         break;

      case SGE_EMA_ADD:
      case SGE_EMA_MOD:
         switch(type) {
            case SGE_TYPE_EXECHOST:
            case SGE_TYPE_CONFIG:
               key = event.get<ET_strkey>();
               ep = key != nullptr ? master_list.find_first(key_nm, ocs::tom::Host(key)) : nullptr;
               if(ep == nullptr) {
                  ERROR("%s element with id " SFQ " not found\n", object_type_get_name(type), key);
                  ret = SGE_EMA_FAILURE;
               }
               if (ret == SGE_EMA_OK) {
                  spool_write_object(&answer_list, context, ep, key, type);
                  answer_list_output(&answer_list);
               }
               break;

            case SGE_TYPE_CALENDAR:
            case SGE_TYPE_CKPT:
            case SGE_TYPE_PE:
            case SGE_TYPE_PROJECT:
            case SGE_TYPE_CQUEUE:
            case SGE_TYPE_USER:
            case SGE_TYPE_USERSET:
            case SGE_TYPE_HGROUP:
               key = event.get<ET_strkey>();
               ep = key != nullptr ? master_list.find_first(key_nm, key) : nullptr;
               if(ep == nullptr) {
                  ERROR("%s element with id " SFQ " not found\n", object_type_get_name(type), key);
                  ret = SGE_EMA_FAILURE;
               }
               
               if (ret == SGE_EMA_OK) {
                  spool_write_object(&answer_list, context, ep, key, type);
                  answer_list_output(&answer_list);
               }
               break;

            case SGE_TYPE_SCHEDD_CONF:
               ep = master_list.front();
               if(ep == nullptr) {
                  ERROR("%s element not found\n", object_type_get_name(type));
                  ret = SGE_EMA_FAILURE;
               }
               if (ret == SGE_EMA_OK) {
                  spool_write_object(&answer_list, context, ep, "default", type);
                  answer_list_output(&answer_list);
               }
               break;
            case SGE_TYPE_JATASK:
            case SGE_TYPE_PETASK:
            case SGE_TYPE_JOB:
               {
                  uint32_t job_id, ja_task_id;
                  const char *pe_task_id;
                  const char *job_key;

                  job_id = event.get<ET_intkey>();
                  ja_task_id = event.get<ET_intkey2>();
                  pe_task_id = event.get<ET_strkey>();

                  ep = master_job_list.find_first<JB_job_number>(job_id);
                  job_key = job_get_key(job_id, ja_task_id, pe_task_id, &buffer);
                  spool_write_object(&answer_list, context, ep, job_key, type);
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

/**
 * @brief Spool the cluster data through a dynamically loaded backend and keep it in step with the event mirror
 * @param argc the number of arguments
 * @param argv method, shared library and backend arguments
 * @return EXIT_SUCCESS; exits with a failure when the spooling cannot be set up
 */
int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_sge_spooling");

   ocs::tom::Element<ocs::tom::SPC> spooling_context;
   time_t next_prof_output = 0;
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   sge_evc_class_t *evc = nullptr;

   /* parse commandline parameters */
   if(argc != 4) {
      ERROR("usage: test_sge_spooling <method> <shared lib> <arguments>");
      sge_exit(1);
   }

   if (ocs::gdi::ClientBase::setup_and_enroll(QEVENT, MAIN_THREAD, &answer_list) != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&answer_list);
      sge_exit(1);
   }
   
   sge_setup_sig_handlers(QEVENT);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   // this thread will use the GLOBAL data store
   ocs::DataStore::select_active_ds(ocs::DataStore::Id::GLOBAL);

   if (reresolve_qualified_hostname() != CL_RETVAL_OK) {
      sge_exit(1);
   }

   if (false == sge_gdi2_evc_setup(&evc, EV_ID_SCHEDD, &answer_list, nullptr)) {
      answer_list_output(&answer_list);
      sge_exit(1);
   }

   /** @def defstring
    * @brief The macro argument as a string literal
    * @param str the text to stringify
    */
#define defstring(str) #str

   /* initialize spooling */
   spooling_context =
      ocs::tom::Element<ocs::tom::SPC>(spool_create_dynamic_context(&answer_list, argv[1], argv[2], argv[3]));
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
         next_prof_output = now + 60;
      }
   }

   sge_mirror_shutdown(evc);

   spool_shutdown_context(&answer_list, spooling_context);
   answer_list_output(&answer_list);

   DRETURN(EXIT_SUCCESS);
}
