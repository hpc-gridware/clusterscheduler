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
 *  Copyright: 2003 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "sge_qmaster_process_message.h"

#include <cstring>
#include <unistd.h>

#include "uti/sge_bootstrap.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_Session.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ack.h"

#include "gdi/sge_gdi.h"
#include "gdi/sge_security.h"
#include "gdi/sge_gdi_packet_pb_cull.h"
#include "gdi/sge_gdi_packet_internal.h"
#include "sgeobj/ocs_Version.h"

#include "comm/commlib.h"

#include "spool/sge_spooling.h"

#include "evm/sge_event_master.h"

#include "sge_c_gdi.h"
#include "sge_qmaster_main.h"
#include "msg_qmaster.h"
#include "msg_common.h"

#include <sge_manop.h>

static void
do_gdi_packet(struct_msg_t *aMsg, monitoring_t *monitor);

static void
do_report_request(struct_msg_t *, monitoring_t *monitor);

static void
do_event_client_exit(struct_msg_t *aMsg, monitoring_t *monitor);

static void
sge_c_job_ack(const char *host, const char *commproc, u_long32 ack_tag, u_long32 ack_ulong,
              u_long32 ack_ulong2, const char *ack_str, monitoring_t *monitor);

/*
 * Prevent these functions made inline by compiler. This is 
 * necessary for Solaris 10 dtrace pid provider to work.
 */
#ifdef SOLARIS
#pragma no_inline(do_gdi_packet, do_c_ack, do_report_request)
#endif

/**
 * @brief Stores a packet in a request queue for further processing by a different thread
 *
 * @param message The message containing the data required to create a packet/task
 * @param data_list The list containing the request data (GDI objects/Report or ACK data)
 * @param type The type of the packet
 */
static void
ocs_store_packet(const struct_msg_t *message, lList *data_list, gdi_packet_request_type_t type) {
   // create a GDI packet to transport the list to a thread that is able to handle it
   sge_gdi_packet_class_t *packet = sge_gdi_packet_create_base(nullptr);
   strcpy(packet->host, message->snd_host);
   strcpy(packet->commproc, message->snd_name);
   packet->commproc_id = message->snd_id;
   packet->response_id = message->request_mid;
   packet->is_intern_request = false;
   packet->request_type = type;
   packet->gdi_session = ocs::SessionManager::GDI_SESSION_NONE;

   // Append a pseudo GDI task
   sge_gdi_packet_append_task(packet, nullptr, ocs::GdiTarget::Target::NO_TARGET, 0, &data_list, nullptr, nullptr, nullptr, false);


   // Put the packet into the task queue so that workers can handle it
   sge_tq_store_notify(GlobalRequestQueue, SGE_TQ_GDI_PACKET, packet);
}

/**
 * @brief Handles an incoming ACK request.
 *
 * After validation of the sender, the ACK request is stored in the task queue for further processing by a worker thread.
 *
 * @param message The message containing the ACK request
 * @param monitor The monitoring object
 */
static void
do_c_ack_request(struct_msg_t *message, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   while (pb_unused(&(message->buf)) > 0) {
      lListElem *ack = nullptr;

      // get the ACK element
      if (cull_unpack_elem(&message->buf, &ack, nullptr)) {
         ERROR("failed unpacking ACK");
         DRETURN_VOID;
      }

      // check the tag and the sender
      u_long32 ack_tag = lGetUlong(ack, ACK_type);
      if (ack_tag == ACK_SIGJOB || ack_tag == ACK_SIGQUEUE) {
         // accept only ack requests from admin or root
         const char *admin_user = bootstrap_get_admin_user();
         const char *component = component_get_component_name();
         if (!sge_security_verify_unique_identifier(true, admin_user, component, 0,
                                                    message->snd_host, message->snd_name, message->snd_id)) {
            ERROR("ACK request from unexpected sender");
            lFreeElem(&ack);
            DRETURN_VOID;
         }
      } else if (ack_tag == ACK_EVENT_DELIVERY) {
         // TODO: here we should check if the event belongs to the user who send the request
         ;
      } else {
         // unknown tag
         ERROR(MSG_COM_UNKNOWN_TAG, sge_u32c(ack_tag));
         lFreeElem(&ack);
         DRETURN_VOID;
      }

      // store the request for a handling thread
      if (ack_tag == ACK_EVENT_DELIVERY) {
         // event ACKs are handled in the event master thread
         // sge_handle_event_ack stores them into the event master message queue
         u_long32 ack_ulong = lGetUlong(ack, ACK_id);
         u_long32 ack_ulong2 = lGetUlong(ack, ACK_id2);
         sge_handle_event_ack(ack_ulong2, (ev_event) ack_ulong);
         lFreeElem(&ack);
      } else {
         // sge_c_ack() will be called by the worker thread to execute the ACK request stored here
         lList *ack_list = lCreateList("ACK list", lGetElemDescr(ack));
         lAppendElem(ack_list, ack);
         ocs_store_packet(message, ack_list, PACKET_ACK_REQUEST);
      }
   }
}


/****** qmaster/sge_qmaster_process_message/sge_qmaster_process_message() ******
*  NAME
*     sge_qmaster_process_message() -- Entry point for qmaster message handling
*
*  SYNOPSIS
*     void* sge_qmaster_process_message(void *anArg) 
*
*  FUNCTION
*     Get a pending message. Handle message based on message tag.
*
*  INPUTS
*     void *anArg - none 
*
*  RESULT
*     void* - none
*
*  NOTES
*     MT-NOTE: thread safety needs to be verified!
*     MT-NOTE:
*     MT-NOTE: This function should only be used as a 'thread function'
*
*******************************************************************************/
void
sge_qmaster_process_message(monitoring_t *monitor) {
   int res;
   struct_msg_t msg{};

   DENTER(TOP_LAYER);

   /*
    * INFO (CR)  
    *
    * The not syncron sge_get_any_request() call will not raise cpu usage to 100%
    * because sge_get_any_request() is doing a cl_commlib_trigger() which will
    * return after the timeout specified at cl_com_create_handle() call in prepare_enroll()
    * which is set to 1 second. A syncron receive would result in a unnecessary qmaster shutdown
    * timeout (syncron receive timeout) when no messages are there to read.
    *
    */

   MONITOR_IDLE_TIME((

                             res = sge_gdi_get_any_request(msg.snd_host, msg.snd_name,
                                                            &msg.snd_id, &msg.buf, &msg.tag, 1, 0, &msg.request_mid)

                     ), monitor, mconf_get_monitoring_options());

   MONITOR_MESSAGES(monitor);

   if (res == CL_RETVAL_OK) {
      switch (msg.tag) {
         case TAG_GDI_REQUEST:
            MONITOR_INC_GDI(monitor);
            do_gdi_packet(&msg, monitor);
            break;
         case TAG_ACK_REQUEST:
            MONITOR_INC_ACK(monitor);
            do_c_ack_request(&msg, monitor);
            break;
         case TAG_EVENT_CLIENT_EXIT:
            MONITOR_INC_ECE(monitor);
            do_event_client_exit(&msg, monitor);
            break;
         case TAG_REPORT_REQUEST:
            MONITOR_INC_REP(monitor);
            do_report_request(&msg, monitor);
            break;
         default:
            DPRINTF("***** UNKNOWN TAG TYPE %d\n", msg.tag);
      }
      clear_packbuffer(&(msg.buf));
   }

   DRETURN_VOID;
} /* sge_qmaster_process_message */

ocs::DataStore::Id
get_most_restrictive_datastore(ocs::DataStore::Id type1, ocs::DataStore::Id type2) {
   if (type1 == type2) {
      return type1;
   } else if ((type1 == ocs::DataStore::LISTENER && type2 == ocs::DataStore::READER) || (type1 == ocs::DataStore::READER && type2 == ocs::DataStore::LISTENER)) {
      return ocs::DataStore::READER;
   } else {
      return ocs::DataStore::GLOBAL;
   }
}

static ocs::DataStore::Id
get_gdi_executor_ds(sge_gdi_packet_class_t *packet) {
   DENTER(TOP_LAYER);

   // Usually the request type defines which DS to be used but there are some exceptions where the global ds
   // is enforced:
   // - Internal GDI requests
   // - DRMAA requests if automatic sessions are disabled (because DRMAA 1 must have a concise view on the data
   //   otherwise the lib will fail).
   // - Execd requests if secondary DS are disabled for execd (only for test purpose)
   if (packet->is_intern_request) {
      // Internal GDI requests will always be executed with access to the GLOBAL data store
      DRETURN(ocs::DataStore::GLOBAL);
   } else if (strcmp(packet->commproc, prognames[DRMAA]) == 0) {
      // DRMAA-requests will only be handled by reader if automatic sessions are enabled
      if (mconf_get_disable_automatic_session()) {
         DRETURN(ocs::DataStore::GLOBAL);
      }
   } else if (strcmp(packet->commproc, prognames[EXECD]) == 0 && mconf_get_disable_secondary_ds_execd()) {
      // request coming from execd should be handled with global DS if secondary DS are disabled for execd
      DRETURN(ocs::DataStore::GLOBAL);
   }

   // check the tasks
   //
   // Assume that the Listener DS is sufficient for the request. Iterate over all tasks and check if
   // the assumption is correct. If READER or GLOBAL DS is required for at least one subtask, then
   // corresponding DS should be used for all sub-tasks.
   ocs::DataStore::Id type = ocs::DataStore::LISTENER;
   for (auto *task : packet->tasks) {
      u_long32 operation = SGE_GDI_GET_OPERATION(task->command);
      u_long32 target = task->target;

      if (operation == SGE_GDI_PERMCHECK) {
         // GDI permission requests to check client user and host permissions can be processed with Listener DS
         type = get_most_restrictive_datastore(type, ocs::DataStore::LISTENER);
      } else if (operation == SGE_GDI_TRIGGER && (target == ocs::GdiTarget::Target::SGE_MASTER_EVENT || target == ocs::GdiTarget::Target::SGE_SC_LIST || target == ocs::GdiTarget::Target::SGE_EV_LIST || target == ocs::GdiTarget::SGE_DUMMY_LIST)) {
         // Also following requests can be processed with Listener DS:
         //    - shutdown request of qmaster (SGE_MASTER_EVENT)
         //    - trigger scheduling (SGE_SC_LIST)
         //    - termination of event client (SGE_EV_LIST)
         //    - start stop of thread (SGE_DUMMY_LIST)
         type = get_most_restrictive_datastore(type, ocs::DataStore::LISTENER);
      } else if (operation == SGE_GDI_GET && (target == ocs::GdiTarget::SGE_EV_LIST || target == ocs::GdiTarget::SGE_DUMMY_LIST)) {
         // show event client list (SGE_EV_LIST); data comes from event master therefor Listener DS possible
         // show thread list (SGE_DUMMY_LIST); data comes from thread main therefor Listener DS possible
         type = get_most_restrictive_datastore(type, ocs::DataStore::LISTENER);
      } else if (operation == SGE_GDI_GET) {
         bool is_qconf = (strcmp(packet->commproc, prognames[QCONF]) == 0);
         const lList *master_manager_list = *ocs::DataStore::get_master_list(SGE_TYPE_MANAGER);
         bool is_manager = manop_is_manager(packet, master_manager_list);

         if (is_qconf && is_manager) {
            // get requests from qconf triggered by managers (e.g. GET request for qconf -mq) require GLOBAL DS to avoid outdated data
            type = get_most_restrictive_datastore(type, ocs::DataStore::GLOBAL);
         } else {
            // other GET requests can be processed with READER DS
            type = get_most_restrictive_datastore(type, ocs::DataStore::READER);
         }
      } else {
         // not handled so far -> use GLOBAL DS
         type = get_most_restrictive_datastore(type, ocs::DataStore::GLOBAL);
      }

      // no need to continue. it cannot get worse than requiring the GLOBAL DS
      if (type == ocs::DataStore::GLOBAL) {
         DRETURN(type);
      }
   }

   DRETURN(type);
}

static void
do_gdi_packet(struct_msg_t *aMsg, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   // unpack the incoming request
   sge_pack_buffer *pb_in = &(aMsg->buf);
   sge_gdi_packet_class_t *packet = nullptr;
   bool local_ret = sge_gdi_packet_unpack(&packet, nullptr, pb_in);
   strcpy(packet->host, aMsg->snd_host);
   strcpy(packet->commproc, aMsg->snd_name);
   packet->commproc_id = aMsg->snd_id;
   packet->response_id = aMsg->request_mid;
   packet->is_intern_request = false;
   packet->request_type = PACKET_GDI_REQUEST;

   // check GDI version
   if (local_ret) {
      local_ret = ocs::Version::do_versions_match(&packet->tasks[0]->answer_list, packet->version, packet->host, packet->commproc, packet->commproc_id);
   }

   // check auth_info (user/group)
   if (local_ret) {
      local_ret = sge_gdi_packet_parse_auth_info(packet, &packet->tasks[0]->answer_list,
                                                 &(packet->uid), packet->user, sizeof(packet->user),
                                                 &(packet->gid), packet->group, sizeof(packet->group),
                                                 &packet->amount, &packet->grp_array);

      packet->gdi_session = ocs::SessionManager::get_session_id(packet->user);
   }

   // check CSP mode if enabled
   if (local_ret) {
      if (!sge_security_verify_user(packet->host, packet->commproc, packet->commproc_id, packet->user)) {
         CRITICAL(MSG_SEC_CRED_SSSI, packet->user, packet->host, packet->commproc, (int) packet->commproc_id);
         answer_list_add(&(packet->tasks[0]->answer_list), SGE_EVENT, STATUS_ENOSUCHUSER, ANSWER_QUALITY_ERROR);
      }
   }

   // handle GDI request limits
   if (local_ret) {
      // TODO handle gdi request limits
      ;
   }

   // handle request specific requirements already here so that we save time potentially in the worker
   //    - manager/operator permissions
   //    - admin/submit/exec host
   if (local_ret) {
      for (auto *task : packet->tasks) {
         local_ret = sge_c_gdi_check_execution_permission(packet, task, monitor);
         if (!local_ret) {
            break;
         }
      }
   }

   // handle errors that might have happened above and then exit
   if (!local_ret) {
      init_packbuffer(&packet->pb, 0, 0);
      for (size_t i = 0; i < packet->tasks.size(); ++i) {
         bool has_next = (i < packet->tasks.size() - 1);
         ocs::GdiTask *task = packet->tasks[i];

         // data might still be that what client sent initially. no need to re-transfer that
         lFreeList(&task->data_list);

         // for all tasks we pack the answer of the first task which contains general errors
         // like version mismatch, auth_info or security issues.
         sge_gdi_packet_pack_task(packet, task, &packet->tasks[0]->answer_list, &packet->pb, has_next);
      }
      sge_gdi_send_any_request(0, nullptr, packet->host, packet->commproc, packet->commproc_id,
                               &packet->pb, TAG_GDI_REQUEST, packet->response_id, nullptr);
      clear_packbuffer(&packet->pb);
      sge_gdi_packet_free(&packet);

      DRETURN_VOID;
   }

   // execute request based on the preferred data store type
   // but do this only is secondary DS are not disabled
   ocs::DataStore::Id ds_type = get_gdi_executor_ds(packet);
   bool ds_enabled = !mconf_get_disable_secondary_ds();
   if (ds_enabled && ds_type == ocs::DataStore::LISTENER) {
      // prepare packbuffer for the clients answer
      init_packbuffer(&(packet->pb), 0, 0);

      // handle the requests
      SGE_LOCK(LOCK_LISTENER, LOCK_READ);
      for (size_t i = 0; i < packet->tasks.size(); ++i) {
         bool has_next = (i < packet->tasks.size() - 1);
         ocs::GdiTask *task = packet->tasks[i];
         sge_c_gdi_process_in_listener(packet, task, &(task->answer_list), monitor, has_next);
      }
      SGE_UNLOCK(LOCK_LISTENER, LOCK_READ);

      // send the response
      MONITOR_MESSAGES_OUT(monitor);
      sge_gdi_send_any_request(0, nullptr, packet->host, packet->commproc, packet->commproc_id,
                               &(packet->pb), TAG_GDI_REQUEST, packet->response_id, nullptr);
      clear_packbuffer(&(packet->pb));
      sge_gdi_packet_free(&packet);
   } else if (ds_enabled && ds_type == ocs::DataStore::READER) {

      // Default is the global request queue unless readers are enabled
      sge_tq_queue_t *queue = GlobalRequestQueue;
      if (!mconf_get_disable_secondary_ds_reader()) {
         u_long64 session_id = ocs::SessionManager::get_session_id(packet->user);

         // Reader DS is enabled so as default we will use the ReaderRequestQueue unless the auto sessions are enabled
         queue = ReaderRequestQueue;
         if (!mconf_get_disable_automatic_session()) {

            // Sessions are enabled so we have to check if the session is up-to-date
            if (!ocs::SessionManager::is_uptodate(session_id)) {
               queue = ReaderWaitingRequestQueue;
            }
         }
      }

      // Store the decision about the DS also in the packet
      packet->ds_type = ds_type;
      sge_tq_store_notify(queue, SGE_TQ_GDI_PACKET, packet);
   } else {
      DTRACE;
      // add to the worker request queue
      packet->ds_type = ds_type;
      sge_tq_store_notify(GlobalRequestQueue, SGE_TQ_GDI_PACKET, packet);
   }

   DRETURN_VOID;
}

/****** sge_qmaster_process_message/do_report_request() ************************
*  NAME
*     do_report_request() -- Process execd load report 
*
*  SYNOPSIS
*     static void do_report_request(struct_msg_t *aMsg) 
*
*  FUNCTION
*     Process execd load reports (TAG_REPORT_REQUEST). Unpack a CULL list with
*     the load report from the pack buffer, which is part of 'aMsg'. Process
*     execd load report.
*
*  INPUTS
*     struct_msg_t *aMsg - execd load report message
*
*  RESULT
*     void - none 
*
*******************************************************************************/
static void
do_report_request(struct_msg_t *aMsg, monitoring_t *monitor) {
   lList *rep = nullptr;
   const char *admin_user = bootstrap_get_admin_user();
   const char *myprogname = component_get_component_name();

   DENTER(TOP_LAYER);

   /* Load reports are only accepted from admin/root user */
   if (!sge_security_verify_unique_identifier(true, admin_user, myprogname, 0,
                                              aMsg->snd_host, aMsg->snd_name, aMsg->snd_id)) {
      DRETURN_VOID;
   }

   if (cull_unpack_list(&(aMsg->buf), &rep)) {
      ERROR(SFNMAX, MSG_CULL_FAILEDINCULLUNPACKLISTREPORT);
      DRETURN_VOID;
   }

   // store the request for a handling thread
   ocs_store_packet(aMsg, rep, PACKET_REPORT_REQUEST);

   DRETURN_VOID;
} /* do_report_request */

/****** qmaster/sge_qmaster_process_message/do_event_client_exit() *************
*  NAME
*     do_event_client_exit() -- handle event client exit message 
*
*  SYNOPSIS
*     static void do_event_client_exit(const char *aHost, const char *aSender, 
*     sge_pack_buffer *aBuffer) 
*
*  FUNCTION
*     Handle event client exit message. Extract event client id from pack
*     buffer. Remove event client. 
*
*  INPUTS
*     const char *aHost        - sender 
*     const char *aSender      - communication endpoint 
*     sge_pack_buffer *aBuffer - buffer 
*
*  RESULT
*     void - none 
*
*  NOTES
*     MT-NOTE: do_event_client_exit() is NOT MT safe. 
*
*******************************************************************************/
static void
do_event_client_exit(struct_msg_t *aMsg, monitoring_t *monitor) {
   u_long32 client_id = 0;

   DENTER(TOP_LAYER);

   if (unpackint(&(aMsg->buf), &client_id) != PACK_SUCCESS) {
      ERROR(MSG_COM_UNPACKINT_I, 1);
      DPRINTF("%s: client id unpack failed - host %s - sender %s\n", __func__, aMsg->snd_host, aMsg->snd_name);
      DRETURN_VOID;
   }

   DPRINTF("%s: remove client " sge_u32 " - host %s - sender %s\n", __func__, client_id, aMsg->snd_host, aMsg->snd_name);

   /* 
   ** check for scheduler shutdown if the request comes from admin or root 
   ** TODO: further enhancement could be matching the owner of the event client 
   **       with the request owner
   */
   if (client_id == 1) {
      const char *admin_user = bootstrap_get_admin_user();
      const char *myprogname = component_get_component_name();
      if (false == sge_security_verify_unique_identifier(true, admin_user, myprogname, 0,
                                                         aMsg->snd_host, aMsg->snd_name, aMsg->snd_id)) {
         DRETURN_VOID;
      }
   }

   sge_remove_event_client(client_id);

   DRETURN_VOID;
} /* do_event_client_exit() */

/**
 * @brief handles an ACK request.
 *
 * Requires the global lock to get access to the main data store.
 *
 * Handles:
 *    - an execd sends an ack for a received job
 *    - an execd sends an ack for a signal delivery
 *    - an external event client sends an ack for received events
 *
 * @param packet
 * @param task
 * @param monitor *
 * @param packet Pseudo GDI packet that contains an ACK request
 * @param task Pseudo GDI task containing the ACK list with one ACK element
 * @param monitor Monitoring object
 */
void
sge_c_ack(sge_gdi_packet_class_t *packet, ocs::GdiTask *task, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   // extract information from the task about the ACK
   lList *ack_list = task->data_list;
   const lListElem *ack = lFirst(ack_list);
   u_long32 ack_tag = lGetUlong(ack, ACK_type);
   u_long32 ack_ulong = lGetUlong(ack, ACK_id);
   u_long32 ack_ulong2 = lGetUlong(ack, ACK_id2);
   const char *ack_str = lGetString(ack, ACK_str);
   DPRINTF("ack_ulong=%ld, ack_ulong2=%ld, ack_str=%s\n", ack_ulong, ack_ulong2, ack_str);

   switch (ack_tag) {
   case ACK_SIGJOB:
   case ACK_SIGQUEUE:
      sge_c_job_ack(packet->host, packet->commproc, ack_tag, ack_ulong, ack_ulong2, ack_str, monitor);
      break;

   case ACK_EVENT_DELIVERY:
      // @TODO: is the global lock also here required?
      sge_handle_event_ack(ack_ulong2, (ev_event) ack_ulong);
      break;
   default:
      // not possible. has been filtered by listener thread already
      ;
   }
   DRETURN_VOID;
}

/***************************************************************/
static void
sge_c_job_ack(const char *host, const char *commproc, u_long32 ack_tag,
              u_long32 ack_ulong, u_long32 ack_ulong2, const char *ack_str, monitoring_t *monitor) {
   lList *answer_list = nullptr;

   DENTER(TOP_LAYER);

   if (strcmp(prognames[EXECD], commproc)) {
      ERROR(MSG_COM_ACK_S, commproc);
      DRETURN_VOID;
   }

   switch (ack_tag) {
      case ACK_SIGJOB: {
         lListElem *jep = nullptr;
         lListElem *jatep = nullptr;
         const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);

         DPRINTF("TAG_SIGJOB\n");
         /* ack_ulong is the jobid */
         if (!(jep = lGetElemUlongRW(master_job_list, JB_job_number, ack_ulong))) {
            ERROR(MSG_COM_ACKEVENTFORUNKNOWNJOB_SU, host, sge_u32c(ack_ulong));
            DRETURN_VOID;
         }
         jatep = job_search_task(jep, nullptr, ack_ulong2);
         if (jatep == nullptr) {
            ERROR(MSG_COM_ACKEVENTFORUNKNOWNTASKOFJOB_SUU, host, sge_u32c(ack_ulong2), sge_u32c(ack_ulong));
            DRETURN_VOID;
         }

         DPRINTF("JOB " sge_u32": SIGNAL ACK\n", lGetUlong(jep, JB_job_number));
         lSetUlong(jatep, JAT_pending_signal, 0);
         te_delete_one_time_event(TYPE_SIGNAL_RESEND_EVENT, ack_ulong, ack_ulong2, nullptr);
         {
            dstring buffer = DSTRING_INIT;
            spool_write_object(&answer_list, spool_get_default_context(), jep,
                               job_get_key(lGetUlong(jep, JB_job_number), ack_ulong2, nullptr, &buffer),
                               SGE_TYPE_JOB, true);
            sge_dstring_free(&buffer);
         }
         answer_list_output(&answer_list);

         break;
      }

      case ACK_SIGQUEUE: {
         lListElem *qinstance = nullptr;
         const lListElem *cqueue = nullptr;
         dstring cqueue_name = DSTRING_INIT;
         dstring host_domain = DSTRING_INIT;
         const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

         cqueue_name_split(ack_str, &cqueue_name, &host_domain, nullptr, nullptr);

         cqueue = lGetElemStr(master_cqueue_list, CQ_name, sge_dstring_get_string(&cqueue_name));

         sge_dstring_free(&cqueue_name);

         if (cqueue != nullptr) {
            const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

            qinstance = lGetElemHostRW(qinstance_list, QU_qhostname, sge_dstring_get_string(&host_domain));
         }
         sge_dstring_free(&host_domain);

         if (qinstance == nullptr) {
            ERROR(MSG_COM_ACK_QUEUE_SS, host, ack_str);
            DRETURN_VOID;
         }

         DPRINTF("QUEUE %s: SIGNAL ACK\n", lGetString(qinstance, QU_full_name));

         lSetUlong(qinstance, QU_pending_signal, 0);
         te_delete_one_time_event(TYPE_SIGNAL_RESEND_EVENT, 0, 0, lGetString(qinstance, QU_full_name));
         spool_write_object(&answer_list, spool_get_default_context(), qinstance,
                            lGetString(qinstance, QU_full_name), SGE_TYPE_QINSTANCE, true);
         answer_list_output(&answer_list);
         break;
      }

      default:
         ERROR(MSG_COM_ACK_UNKNOWN_S, host);
   }
   DRETURN_VOID;
}
