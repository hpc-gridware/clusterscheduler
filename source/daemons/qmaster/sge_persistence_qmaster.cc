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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_rmon_macros.h"
#include "uti/sge_component.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_time.h"

#include "cull/cull.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_host.h"

#include "spool/sge_spooling.h"
#include "spool/loader/sge_spooling_loader.h"

#include "basis_types.h"
#include "evm/sge_event_master.h"
#include "sge_persistence_qmaster.h"
#include "msg_qmaster.h"

static unsigned long spooling_wait_time = 0;

bool
sge_initialize_persistence(lList **answer_list) {
   bool ret = true;

   lListElem *spooling_context;
   const char *spooling_method = bootstrap_get_spooling_method();
   const char *spooling_lib = bootstrap_get_spooling_lib();
   const char *spooling_params = bootstrap_get_spooling_params();

   DENTER(TOP_LAYER);

   if (getenv("SGE_TEST_SPOOLING_WAIT_TIME") != nullptr) {
      spooling_wait_time = atoi(getenv("SGE_TEST_SPOOLING_WAIT_TIME"));
   }

   /* create spooling context */
   spooling_context = spool_create_dynamic_context(answer_list, spooling_method, spooling_lib, spooling_params);
   if (spooling_context == nullptr) {
      /* error message created in spool_create_dynamic_context */
      ret = false;
   } else {
      /* set options: enable recovery at startup (bdb) */
      spool_set_option(answer_list, spooling_context, "recover=true");

      /* startup spooling context */
      if (!spool_startup_context(answer_list, spooling_context, true)) {
         /* error message created in spool_startup_context */
         ret = false;
      } else {
         /* set this context as default */
         spool_set_default_context(spooling_context);
      }
   }

   DRETURN(ret);
}

void
sge_initialize_persistance_timer() {
   te_event_t ev = nullptr;

   DENTER(TOP_LAYER);

   te_register_event_handler(spooling_trigger_handler, TYPE_SPOOLING_TRIGGER);

   ev = te_new_event(sge_get_gmt64(), TYPE_SPOOLING_TRIGGER, ONE_TIME_EVENT, 0, 0, nullptr);
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
}

bool
sge_shutdown_persistence(lList **answer_list) {
   bool ret = true;
   u_long64 time = 0;
   lList *alp = nullptr;
   lListElem *context;

   DENTER(TOP_LAYER);

   /* trigger spooling actions (flush data) */
   if (!spool_trigger_context(&alp, spool_get_default_context(), 0, &time)) {
      answer_list_output(&alp);
   }

   /* shutdown spooling */
   context = spool_get_default_context();
   if (context != nullptr) {
      lList *local_answer = nullptr;

      if (answer_list != nullptr) {
         local_answer = *answer_list;
      }

      spool_shutdown_context(&local_answer, context);
      if (answer_list == nullptr) {
         answer_list_output(&local_answer);
      }

      lFreeElem(&context);
      spool_set_default_context(context);
   }

   DRETURN(ret);
}

void
spooling_trigger_handler(te_event_t anEvent, monitoring_t *monitor) {
   u_long64 next_trigger = 0;
   lList *answer_list = nullptr;
   te_event_t ev = nullptr;

   DENTER(TOP_LAYER);

   /* trigger spooling regular actions */
   if (!spool_trigger_context(&answer_list, spool_get_default_context(),
                              te_get_when(anEvent), &next_trigger)) {
      answer_list_output(&answer_list);
   }

   /* validate next_trigger. If it is invalid, set it to one minute after now */
   u_long64 now = sge_get_gmt64();
   if (next_trigger <= now) {
      next_trigger = now + sge_gmt32_to_gmt64(60);
   }

   /* set timerevent for next trigger */
   ev = te_new_event(next_trigger, te_get_type(anEvent), ONE_TIME_EVENT, 0, 0, nullptr);
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
}

/****** sge_persistence_qmaster/sge_event_spool() ******************************
*  NAME
*     sge_event_spool() -- send event and spool
*
*  SYNOPSIS
*     bool 
*     sge_event_spool(lList **answer_list, u_long64 timestamp, ev_event event,
*                     u_long32 intkey1, u_long32 intkey2, const char *strkey, 
*                     const char *strkey2, const char *session,
*                     lListElem *object, lListElem *sub_object1, 
*                     lListElem *sub_object2, bool send_event, bool spool) 
*
*  FUNCTION
*     Spools (writes or deletes) an object.
*     If spooling was successful, send the given event.
*     Finally, the changed bits (information, which fields of the object
*     and it's subobjects were changed) is cleared.
*
*  INPUTS
*     lList **answer_list    - to return error messages
*     u_long64 timestamp     - timestamp of object change, if 0 is passed,
*                              use current date/time
*     ev_event event         - the event to send
*     u_long32 intkey1       - an integer key (job_id)
*     u_long32 intkey2       - a second integer key (ja_task_id)
*     const char *strkey     - a string key (all other keys)
*     const char *strkey2    - a string key (all other keys)
*     const char *session    - events session key
*     lListElem *object      - the object to spool and send
*     lListElem *sub_object1 - optionally a sub object (ja_task)
*     lListElem *sub_object2 - optionally a sub sub object (pe_task)
*     bool send_event        - shall we send an event, or only spool?
*     bool spool             - shall we spool or only send an event?
*
*  RESULT
*     bool - true on success, 
*            false on error. answer_list will contain an error description 
*  NOTES
*     From an academic standpoint, the parameter spool shouldn't be needed.
*     Whenever an object changes and a change event is created, the data 
*     basis should also be updated (spooled).
*
*  BUGS
*
*  SEE ALSO
*     
*******************************************************************************/
bool
sge_event_spool(lList **answer_list, u_long64 timestamp, ev_event event, u_long32 intkey1,
                u_long32 intkey2, const char *strkey, const char *strkey2, const char *session, lListElem *object,
                lListElem *sub_object1, lListElem *sub_object2, bool send_event, bool spool, u_long64 gdi_session) {
   bool ret = true;
   const char *key = nullptr;
   sge_object_type object_type;
   lListElem *element = nullptr;
   bool do_delete = false;
   dstring buffer = DSTRING_INIT;

   DENTER(TOP_LAYER);

   /*for testing a fixed gid_error, this has been introduced. We need it to slowdown*/
   /*the spooling mechanism, to simulate the situation where this error appears*/
   if (spooling_wait_time != 0) {
      unsigned long sleep_time = spooling_wait_time;
      bool do_sleep = false;
      do {
         /*
          * find out if there is a qping -dump client connected to qmaster
          */
         cl_com_handle_t *handle = cl_com_get_handle("qmaster", 1);
         if (handle != nullptr) {
            if (handle->debug_client_setup != nullptr) {
               if (handle->debug_client_setup->dc_mode != CL_DEBUG_CLIENT_OFF) {
                  do_sleep = true;
               } else {
                  do_sleep = false;
               }
            }
         }

         if (do_sleep) {
            usleep(1000000);
            sleep_time--;
         }
      } while (sleep_time > 0 && do_sleep);
   }

   switch (event) {
      case sgeE_ADMINHOST_LIST:
      case sgeE_ADMINHOST_ADD:
      case sgeE_ADMINHOST_DEL:
      case sgeE_ADMINHOST_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_ADMINHOST;
         break;
      case sgeE_CALENDAR_LIST:
      case sgeE_CALENDAR_ADD:
      case sgeE_CALENDAR_DEL:
      case sgeE_CALENDAR_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_CALENDAR;
         break;
      case sgeE_CKPT_LIST:
      case sgeE_CKPT_ADD:
      case sgeE_CKPT_DEL:
      case sgeE_CKPT_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_CKPT;
         break;
      case sgeE_CENTRY_LIST:
      case sgeE_CENTRY_ADD:
      case sgeE_CENTRY_DEL:
      case sgeE_CENTRY_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_CENTRY;
         break;
      case sgeE_CONFIG_LIST:
      case sgeE_CONFIG_ADD:
      case sgeE_CONFIG_DEL:
      case sgeE_CONFIG_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_CONFIG;
         break;
      case sgeE_EXECHOST_LIST:
      case sgeE_EXECHOST_ADD:
      case sgeE_EXECHOST_DEL:
      case sgeE_EXECHOST_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_EXECHOST;
         break;
      case sgeE_JATASK_ADD:
      case sgeE_JATASK_DEL:
      case sgeE_JATASK_MOD:
         key = job_get_key(intkey1, intkey2, strkey, &buffer);
         element = sub_object1;
         object_type = SGE_TYPE_JATASK;
         break;
      case sgeE_PETASK_ADD:
      case sgeE_PETASK_DEL:
         key = job_get_key(intkey1, intkey2, strkey, &buffer);
         element = sub_object2;
         object_type = SGE_TYPE_PETASK;
         break;
      case sgeE_JOB_LIST:
      case sgeE_JOB_ADD:
      case sgeE_JOB_DEL:
      case sgeE_JOB_MOD:
      case sgeE_JOB_USAGE:
      case sgeE_JOB_FINAL_USAGE:
      case sgeE_JOB_FINISH:
         key = job_get_key(intkey1, intkey2, strkey, &buffer);
         element = object;
         object_type = SGE_TYPE_JOB;
         break;
      case sgeE_JOB_SCHEDD_INFO_LIST:
      case sgeE_JOB_SCHEDD_INFO_ADD:
      case sgeE_JOB_SCHEDD_INFO_DEL:
      case sgeE_JOB_SCHEDD_INFO_MOD:
         key = job_get_key(intkey1, intkey2, strkey, &buffer);
         element = object;
         object_type = SGE_TYPE_JOB_SCHEDD_INFO;
         break;
      case sgeE_MANAGER_LIST:
      case sgeE_MANAGER_ADD:
      case sgeE_MANAGER_DEL:
      case sgeE_MANAGER_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_MANAGER;
         break;
      case sgeE_OPERATOR_LIST:
      case sgeE_OPERATOR_ADD:
      case sgeE_OPERATOR_DEL:
      case sgeE_OPERATOR_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_OPERATOR;
         break;
      case sgeE_NEW_SHARETREE:
         /* we have only one sharetree - there is no key */
         key = "sharetree";
         element = object;
         object_type = SGE_TYPE_SHARETREE;
         break;
      case sgeE_PE_LIST:
      case sgeE_PE_ADD:
      case sgeE_PE_DEL:
      case sgeE_PE_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_PE;
         break;
      case sgeE_PROJECT_LIST:
      case sgeE_PROJECT_ADD:
      case sgeE_PROJECT_DEL:
      case sgeE_PROJECT_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_PROJECT;
         break;
      case sgeE_ACK_TIMEOUT:
      case sgeE_QMASTER_GOES_DOWN:
         key = strkey;
         element = object;
         /* nothing to spool for this event */
         object_type = SGE_TYPE_ALL;
         break;
      case sgeE_CQUEUE_LIST:
      case sgeE_CQUEUE_ADD:
      case sgeE_CQUEUE_DEL:
      case sgeE_CQUEUE_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_CQUEUE;
         break;
      case sgeE_QINSTANCE_ADD:
      case sgeE_QINSTANCE_DEL:
      case sgeE_QINSTANCE_MOD:
      case sgeE_QINSTANCE_SOS:
      case sgeE_QINSTANCE_USOS:
         sge_dstring_sprintf(&buffer, SFN "/" SFN, strkey, strkey2);
         key = sge_dstring_get_string(&buffer);
         element = object;
         object_type = SGE_TYPE_QINSTANCE;
         break;
      case sgeE_SCHED_CONF:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_SCHEDD_CONF;
         break;
      case sgeE_SCHEDDMONITOR:
      case sgeE_SHUTDOWN:
         key = strkey;
         element = object;
         /* nothing to spool for this event */
         object_type = SGE_TYPE_ALL;
         break;
      case sgeE_SUBMITHOST_LIST:
      case sgeE_SUBMITHOST_ADD:
      case sgeE_SUBMITHOST_DEL:
      case sgeE_SUBMITHOST_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_SUBMITHOST;
         break;
      case sgeE_USER_LIST:
      case sgeE_USER_ADD:
      case sgeE_USER_DEL:
      case sgeE_USER_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_USER;
         break;
      case sgeE_USERSET_LIST:
      case sgeE_USERSET_ADD:
      case sgeE_USERSET_DEL:
      case sgeE_USERSET_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_USERSET;
         break;
      case sgeE_RQS_LIST:
      case sgeE_RQS_ADD:
      case sgeE_RQS_DEL:
      case sgeE_RQS_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_RQS;
         break;
      case sgeE_HGROUP_LIST:
      case sgeE_HGROUP_ADD:
      case sgeE_HGROUP_DEL:
      case sgeE_HGROUP_MOD:
         key = strkey;
         element = object;
         object_type = SGE_TYPE_HGROUP;
         break;
      case sgeE_AR_LIST:
      case sgeE_AR_ADD:
      case sgeE_AR_DEL:
      case sgeE_AR_MOD:
         key = sge_dstring_sprintf(&buffer, sge_u32, intkey1);
         element = object;
         object_type = SGE_TYPE_AR;
         break;
      default:
         /* nothing to spool */
         object_type = SGE_TYPE_ALL;
         ret = false;
         break;
   }

   /* only continue in case of valid event */
   if (ret) {
      switch (event) {
         case sgeE_ADMINHOST_DEL:
         case sgeE_CALENDAR_DEL:
         case sgeE_CKPT_DEL:
         case sgeE_CENTRY_DEL:
         case sgeE_CONFIG_DEL:
         case sgeE_EXECHOST_DEL:
         case sgeE_JATASK_DEL:
         case sgeE_PETASK_DEL:
         case sgeE_JOB_DEL:
         case sgeE_JOB_SCHEDD_INFO_DEL:
         case sgeE_MANAGER_DEL:
         case sgeE_OPERATOR_DEL:
         case sgeE_PE_DEL:
         case sgeE_PROJECT_DEL:
         case sgeE_CQUEUE_DEL:
         case sgeE_QINSTANCE_DEL:
         case sgeE_SUBMITHOST_DEL:
         case sgeE_USER_DEL:
         case sgeE_USERSET_DEL:
         case sgeE_RQS_DEL:
         case sgeE_HGROUP_DEL:
         case sgeE_AR_DEL:
            do_delete = true;
            break;
         case sgeE_NEW_SHARETREE:
            if (object == nullptr) {
               do_delete = true;
            }
            break;
         default:
            do_delete = false;
            break;
      }

      /* if spooling was requested and we have an object type to spool */
      if (spool && object_type != SGE_TYPE_ALL) {
         /* use an own answer list for the low level spooling operation. 
          * in case of error, generate a high level error message.
          */
         lList *spool_answer_list = nullptr;
         if (do_delete) {
            ret = spool_delete_object(&spool_answer_list, spool_get_default_context(),
                                      object_type, key, true);
         } else {
            lList *tmp_list = nullptr;
            const lListElem *load_value;

            /* 
             *  Only static load values should be spooled, therefore we modify
             *  the host elem to spool
             */
            switch (event) {
               case sgeE_EXECHOST_LIST:
               case sgeE_EXECHOST_ADD:
               case sgeE_EXECHOST_MOD:
                  tmp_list = lCreateList("", HL_Type);
                  for_each_ep(load_value, lGetList(object, EH_load_list)) {
                     if (lGetBool(load_value, HL_is_static)) {
                        lAppendElem(tmp_list, lCopyElem(load_value));
                     }
                  }
                  lXchgList(object, EH_load_list, &tmp_list);
                  break;
               default:
                  break;
            }

            ret = spool_write_object(&spool_answer_list, spool_get_default_context(),
                                     element, key, object_type, true);

            switch (event) {
               case sgeE_EXECHOST_LIST:
               case sgeE_EXECHOST_ADD:
               case sgeE_EXECHOST_MOD:
                  lXchgList(object, EH_load_list, &tmp_list);
                  lFreeList(&tmp_list);
                  break;
               default:
                  break;
            }
         }
         /* output low level error messages */
         answer_list_output(&spool_answer_list);

         /* in case of error: generate error message for caller */
         if (!ret) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                    ANSWER_QUALITY_ERROR,
                                    do_delete ?
                                    MSG_PERSISTENCE_DELETE_FAILED_S :
                                    MSG_PERSISTENCE_WRITE_FAILED_S,
                                    key);

         }
      }
   }

   /* send event only, if spooling succeeded */
   if (ret) {
      if (send_event) {
         sge_add_event(timestamp, event, intkey1, intkey2, strkey, strkey2, session, element, gdi_session);
      }
   }

   sge_dstring_free(&buffer);

   DRETURN(ret);
}

