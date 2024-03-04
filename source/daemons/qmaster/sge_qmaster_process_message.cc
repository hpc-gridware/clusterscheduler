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

#include "comm/commlib.h"

#include "spool/sge_spooling.h"

#include "evm/sge_event_master.h"

#include "sge_qmaster_main.h"
#include "sge_thread_worker.h"
#include "msg_qmaster.h"
#include "msg_common.h"

static void
do_gdi_packet(lList **answer_list, struct_msg_t *aMsg, monitoring_t *monitor);

static void
do_c_ack(struct_msg_t *aMsg, monitoring_t *monitor);

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
   struct_msg_t msg;

   DENTER(TOP_LAYER);

   memset((void *) &msg, 0, sizeof(struct_msg_t));

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

                     ), monitor, mconf_get_monitor_time(), mconf_is_monitor_message());

   MONITOR_MESSAGES(monitor);

   if (res == CL_RETVAL_OK) {
      switch (msg.tag) {
         case TAG_GDI_REQUEST:
            MONITOR_INC_GDI(monitor);
            do_gdi_packet(nullptr, &msg, monitor);
            break;
         case TAG_ACK_REQUEST:
            MONITOR_INC_ACK(monitor);
            do_c_ack(&msg, monitor);
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
            DPRINTF(("***** UNKNOWN TAG TYPE %d\n", msg.tag));
      }
      clear_packbuffer(&(msg.buf));
   }

   DRETURN_VOID;
} /* sge_qmaster_process_message */

static void
do_gdi_packet(lList **answer_list, struct_msg_t *aMsg, monitoring_t *monitor) {
   sge_pack_buffer *pb_in = &(aMsg->buf);
   sge_gdi_packet_class_t *packet = nullptr;
   bool local_ret;

   DENTER(TOP_LAYER);

   /*
    * unpack the packet and set values 
    */
   local_ret = sge_gdi_packet_unpack(&packet, answer_list, pb_in);
   packet->host = sge_strdup(nullptr, aMsg->snd_host);
   packet->commproc = sge_strdup(nullptr, aMsg->snd_name);
   packet->commproc_id = aMsg->snd_id;
   packet->response_id = aMsg->request_mid;
   packet->is_intern_request = false;
   packet->is_gdi_request = true;

   /* 
    * Security checks:
    *    check version 
    *    check auth_info (user)
    *    check host, commproc 
    */
   if (local_ret) {
      local_ret = sge_gdi_packet_verify_version(packet, &(packet->first_task->answer_list));
   }
   if (local_ret) {
      local_ret = sge_gdi_packet_parse_auth_info(packet, &(packet->first_task->answer_list),
                                                 &(packet->uid), packet->user, sizeof(packet->user),
                                                 &(packet->gid), packet->group, sizeof(packet->group));
   }
   if (local_ret) {
      const char *admin_user = bootstrap_get_admin_user();
      const char *progname = component_get_component_name();

      if (!sge_security_verify_user(packet->host, packet->commproc,
                                    packet->commproc_id, admin_user, packet->user, progname)) {
         CRITICAL(MSG_SEC_CRED_SSSI, packet->user, packet->host, packet->commproc, (int) packet->commproc_id);
         answer_list_add(&(packet->first_task->answer_list), SGE_EVENT, STATUS_ENOSUCHUSER, ANSWER_QUALITY_ERROR);
      }
   }

   /*
    * EB: TODO: CLEANUP: Handle permission checks in listener not in worker thread.
    *
    * This would be the correct place to handle all permissions   
    * checks which are currently done inside of sge_c_gdi.
    * This can only be done if it is not neccessary anymore to pass a 
    * packbuffer to a worker thread.
    */

   /*
    * Put the packet into the queue so that a worker can handle it
    */
   sge_tq_store_notify(Master_Task_Queue, SGE_TQ_GDI_PACKET, packet);
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

   /*
    * create a GDI packet to transport the list to the worker where
    * it will be handled
    */
   sge_gdi_packet_class_t *packet = sge_gdi_packet_create_base(nullptr);
   packet->host = sge_strdup(nullptr, aMsg->snd_host);
   packet->commproc = sge_strdup(nullptr, aMsg->snd_name);
   packet->commproc_id = aMsg->snd_id;
   packet->response_id = aMsg->request_mid;
   packet->is_intern_request = false;
   packet->is_gdi_request = false;

   /* 
    * Append a pseudo GDI task
    */
   sge_gdi_packet_append_task(packet, nullptr, 0, 0, &rep, nullptr, nullptr, nullptr, false);

   /*
    * Put the packet into the task queue so that workers can handle it
    */
   sge_tq_store_notify(Master_Task_Queue, SGE_TQ_GDI_PACKET, packet);

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
      DPRINTF(("%s: client id unpack failed - host %s - sender %s\n", __func__, aMsg->snd_host, aMsg->snd_name));
      DRETURN_VOID;
   }

   DPRINTF(("%s: remove client " sge_u32 " - host %s - sender %s\n", __func__, client_id, aMsg->snd_host, aMsg->snd_name));

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


/****************************************************
 Master code.
 Handle ack requests
 Ack requests can be packed together in one packet.

 These are:
 - an execd sends an ack for a received job
 - an execd sends an ack for a signal delivery
 - the schedd sends an ack for received events

 if the counterpart uses the dispatcher ack_tag is the
 TAG we sent to the counterpart.
 ****************************************************/
static void
do_c_ack(struct_msg_t *aMsg, monitoring_t *monitor) {
   u_long32 ack_tag, ack_ulong, ack_ulong2;
   const char *admin_user = bootstrap_get_admin_user();
   const char *myprogname = component_get_component_name();
   lListElem *ack = nullptr;

   DENTER(TOP_LAYER);

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   /* Do some validity tests */
   while (pb_unused(&(aMsg->buf)) > 0) {
      if (cull_unpack_elem(&(aMsg->buf), &ack, nullptr)) {
         ERROR("failed unpacking ACK");
         SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE)
         DRETURN_VOID;
      }
      ack_tag = lGetUlong(ack, ACK_type);
      ack_ulong = lGetUlong(ack, ACK_id);
      ack_ulong2 = lGetUlong(ack, ACK_id2);

      DPRINTF(("ack_ulong = %ld, ack_ulong2 = %ld\n", ack_ulong, ack_ulong2));
      switch (ack_tag) { /* send by dispatcher */
         case ACK_SIGJOB:
         case ACK_SIGQUEUE:
            MONITOR_EACK(monitor);
            /*
            ** accept only ack requests from admin or root
            */
            if (false == sge_security_verify_unique_identifier(true, admin_user, myprogname, 0,
                                                               aMsg->snd_host, aMsg->snd_name, aMsg->snd_id)) {
               SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE)
               DRETURN_VOID;
            }
            /* an execd sends a job specific acknowledge ack_ulong == jobid of received job */
            sge_c_job_ack(aMsg->snd_host, aMsg->snd_name, ack_tag, ack_ulong, ack_ulong2, lGetString(ack, ACK_str),
                          monitor);
            break;

         case ACK_EVENT_DELIVERY:
            /*
            ** TODO: in this case we should check if the event belongs to the user
            **       who send the request
            */
            sge_handle_event_ack(ack_ulong2, (ev_event) ack_ulong);
            break;

         default:
            WARNING(MSG_COM_UNKNOWN_TAG, sge_u32c(ack_tag));
            break;
      }
      lFreeElem(&ack);
   }

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE)
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
         const lList *master_job_list = *object_type_get_master_list(SGE_TYPE_JOB);

         DPRINTF(("TAG_SIGJOB\n"));
         /* ack_ulong is the jobid */
         if (!(jep = lGetElemUlongRW(master_job_list, JB_job_number, ack_ulong))) {
            ERROR(MSG_COM_ACKEVENTFORUNKOWNJOB_U, sge_u32c(ack_ulong));
            DRETURN_VOID;
         }
         jatep = job_search_task(jep, nullptr, ack_ulong2);
         if (jatep == nullptr) {
            ERROR(MSG_COM_ACKEVENTFORUNKNOWNTASKOFJOB_UU, sge_u32c(ack_ulong2), sge_u32c(ack_ulong));
            DRETURN_VOID;
         }

         DPRINTF(("JOB " sge_u32": SIGNAL ACK\n", lGetUlong(jep, JB_job_number)));
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
         const lList *master_cqueue_list = *object_type_get_master_list(SGE_TYPE_CQUEUE);

         cqueue_name_split(ack_str, &cqueue_name, &host_domain, nullptr, nullptr);

         cqueue = lGetElemStr(master_cqueue_list, CQ_name, sge_dstring_get_string(&cqueue_name));

         sge_dstring_free(&cqueue_name);

         if (cqueue != nullptr) {
            const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

            qinstance = lGetElemHostRW(qinstance_list, QU_qhostname, sge_dstring_get_string(&host_domain));
         }
         sge_dstring_free(&host_domain);

         if (qinstance == nullptr) {
            ERROR(MSG_COM_ACK_QUEUE_S, ack_str);
            DRETURN_VOID;
         }

         DPRINTF(("QUEUE %s: SIGNAL ACK\n", lGetString(qinstance, QU_full_name)));

         lSetUlong(qinstance, QU_pending_signal, 0);
         te_delete_one_time_event(TYPE_SIGNAL_RESEND_EVENT, 0, 0, lGetString(qinstance, QU_full_name));
         spool_write_object(&answer_list, spool_get_default_context(), qinstance,
                            lGetString(qinstance, QU_full_name), SGE_TYPE_QINSTANCE, true);
         answer_list_output(&answer_list);
         break;
      }

      default:
         ERROR(SFNMAX, MSG_COM_ACK_UNKNOWN);
   }
   DRETURN_VOID;
}
