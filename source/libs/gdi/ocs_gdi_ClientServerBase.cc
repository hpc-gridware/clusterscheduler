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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_profiling.h"
#include "uti/sge_hostname.h"

#include "gdi/msg_gdilib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_feature.h"

#include "ocs_gdi_ClientServerBase.h"

/************************************************************
   TODO: rewrite this function
   COMMLIB/SECURITY WRAPPERS
   FIXME: FUNCTIONPOINTERS SHOULD BE SET IN sge_security_initialize !!!

   Test dlopen functionality, stub libs or check if openssl calls can be added
   without infringing a copyright

   NOTES
      MT-NOTE: gdi_send_message() is MT safe (assumptions)
*************************************************************/
int
ocs::gdi::ClientServerBase::gdi_send_message(int synchron, const char *tocomproc, int toid, const char *tohost, int tag, char **buffer, int buflen, u_long32 *mid) {
   DENTER(GDI_LAYER);
   int ret;
   cl_com_handle_t *handle = nullptr;
   cl_xml_ack_type_t ack_type = CL_MIH_MAT_NAK;
   unsigned long dummy_mid;
   unsigned long *mid_pointer = nullptr;
   int use_execd_handle = 0;
   u_long32 progid = component_get_component_id();


   /* CR- TODO: This is for tight integration of qrsh -inherit
    *
    *       All GDI functions normally connect to qmaster, but
    *       qrsh -inhert want's to talk to execd. A second handle
    *       is created. All gdi functions should accept a pointer
    *       to a cl_com_handle_t* handle and use this handle to
    *       send/receive messages to the correct endpoint.
    */
   if (tocomproc[0] == '\0') {
      DEBUG("tocomproc is empty string\n");
   }
   switch (progid) {
      case QMASTER:
      case EXECD:
         use_execd_handle = 0;
         break;
      default:
         if (strcmp(tocomproc, prognames[QMASTER]) == 0) {
            use_execd_handle = 0;
         } else {
            if (tocomproc != nullptr && tocomproc[0] != '\0') {
               use_execd_handle = 1;
            }
         }
   }


   if (use_execd_handle == 0) {
      /* normal gdi send to qmaster */
      DEBUG("standard gdi request to qmaster\n");
      handle = cl_com_get_handle(component_get_component_name(), 0);
   } else {
      /* we have to send a message to another component than qmaster */
      DEBUG("search handle to \"%s\"\n", tocomproc);
      handle = cl_com_get_handle("execd_handle", 0);
      if (handle == nullptr) {
         int commlib_error = CL_RETVAL_OK;
         cl_framework_t communication_framework = CL_CT_TCP;
         DEBUG("creating handle to \"%s\"\n", tocomproc);
         if (feature_is_enabled(FEATURE_CSP_SECURITY)) {
            DPRINTF("using communication lib with SSL framework (execd_handle)\n");
            communication_framework = CL_CT_SSL;
         }
         cl_com_create_handle(&commlib_error, communication_framework, CL_CM_CT_MESSAGE,
                              false, sge_get_execd_port(), CL_TCP_DEFAULT,
                              "execd_handle", 0, 0, 500);
         handle = cl_com_get_handle("execd_handle", 0);
         if (handle == nullptr) {
            ERROR(MSG_GDI_CANT_CREATE_HANDLE_TOEXECD_S, tocomproc);
            ERROR(SFNMAX, cl_get_error_text(commlib_error));
         }
      }
   }

   if (synchron) {
      ack_type = CL_MIH_MAT_ACK;
   }
   if (mid != nullptr) {
      mid_pointer = &dummy_mid;
   }

   ret = cl_commlib_send_message(handle, (char *) tohost, (char *) tocomproc, toid, ack_type, (cl_byte_t **) buffer,
                                 (unsigned long) buflen, mid_pointer, 0, tag, false, (bool) synchron);

   if (mid != nullptr) {
      *mid = dummy_mid;
   }

   DRETURN(ret);
}

/**********************************************************************
  send a message giving a packbuffer

  same as gdi_send_message, but this is delivered a sge_pack_buffer.
  this function flushes the z_stream_buffer if compression is turned on
  and passes the result on to send_message
  Always use this function instead of gdi_send_message directly, even
  if compression is turned off.

    NOTES
       MT-NOTE: gdi_send_message_pb() is MT safe (assumptions)
**********************************************************************/
int
ocs::gdi::ClientServerBase::gdi_send_message_pb(int synchron, const char *tocomproc, int toid, const char *tohost,
                                            ClientServerBaseTag tag, sge_pack_buffer *pb, u_long32 *mid) {
   DENTER(GDI_LAYER);
   int ret;
   if (!pb) {
      DPRINTF("no pointer for sge_pack_buffer\n");
      ret = gdi_send_message(synchron, tocomproc, toid, tohost, tag, nullptr, 0, mid);
      DRETURN(ret);
   }
   ret = gdi_send_message(synchron, tocomproc, toid, tohost, tag, &pb->head_ptr, pb->bytes_used, mid);
   DRETURN(ret);
}

/*
 *  TODO: rewrite this function
 *  NOTES
 *     MT-NOTE: gdi_receive_message() is MT safe (major assumptions!)
 *
 */
int
ocs::gdi::ClientServerBase::gdi_receive_message(char *fromcommproc, u_short *fromid, char *fromhost,
                                            ClientServerBaseTag *tag, char **buffer, u_long32 *buflen, int synchron) {

   int ret;
   cl_com_handle_t *handle = nullptr;
   cl_com_message_t *message = nullptr;
   cl_com_endpoint_t *sender = nullptr;
   int use_execd_handle = 0;

   u_long32 progid = component_get_component_id();
   u_long32 sge_execd_port = bootstrap_get_sge_execd_port();

   DENTER(GDI_LAYER);

   /* CR- TODO: This is for tight integration of qrsh -inherit
 *
 *       All GDI functions normally connect to qmaster, but
 *       qrsh -inhert want's to talk to execd. A second handle
 *       is created. All gdi functions should accept a pointer
 *       to a cl_com_handle_t* handle and use this handle to
 *       send/receive messages to the correct endpoint.
 */


   if (fromcommproc[0] == '\0') {
      DEBUG("fromcommproc is empty string\n");
   }
   switch (progid) {
      case QMASTER:
      case EXECD:
         use_execd_handle = 0;
         break;
      default:
         if (strcmp(fromcommproc, prognames[QMASTER]) == 0) {
            use_execd_handle = 0;
         } else {
            if (fromcommproc != nullptr && fromcommproc[0] != '\0') {
               use_execd_handle = 1;
            }
         }
         break;
   }

   if (use_execd_handle == 0) {
      /* normal gdi send to qmaster */
      DEBUG("standard gdi receive message\n");
      handle = cl_com_get_handle(component_get_component_name(), 0);
   } else {
      /* we have to send a message to another component than qmaster */
      DEBUG("search handle to \"%s\"\n", fromcommproc);
      handle = cl_com_get_handle("execd_handle", 0);
      if (handle == nullptr) {
         int commlib_error = CL_RETVAL_OK;
         cl_framework_t communication_framework = CL_CT_TCP;
         DEBUG("creating handle to \"%s\"\n", fromcommproc);
         if (feature_is_enabled(FEATURE_CSP_SECURITY)) {
            DPRINTF("using communication lib with SSL framework (execd_handle)\n");
            communication_framework = CL_CT_SSL;
         }

         cl_com_create_handle(&commlib_error, communication_framework, CL_CM_CT_MESSAGE,
                              false, (int)sge_execd_port, CL_TCP_DEFAULT,
                              "execd_handle", 0, 0, 500);
         handle = cl_com_get_handle("execd_handle", 0);
         if (handle == nullptr) {
            ERROR(MSG_GDI_CANT_CREATE_HANDLE_TOEXECD_S, fromcommproc);
            ERROR(SFNMAX, cl_get_error_text(commlib_error));
         }
      }
   }

   ret = cl_commlib_receive_message(handle, fromhost, fromcommproc, *fromid, (bool) synchron, 0, &message, &sender);

   if (ret == CL_RETVAL_CONNECTION_NOT_FOUND) {
      if (fromcommproc[0] != '\0' && fromhost[0] != '\0') {
         /* The connection was closed, reopen it */
         ret = cl_commlib_open_connection(handle, fromhost, fromcommproc, *fromid);
         INFO("reopen connection to %s,%s," sge_uu32 " (1)\n", fromhost, fromcommproc, static_cast<u_long32>(*fromid));
         if (ret == CL_RETVAL_OK) {
            INFO("reconnected successfully\n");
            ret = cl_commlib_receive_message(handle, fromhost, fromcommproc, *fromid, (bool) synchron, 0, &message,
                                             &sender);
         }
      } else {
         DEBUG("can't reopen a connection to unspecified host or commproc (1)\n");
      }
   }

   if (message != nullptr && ret == CL_RETVAL_OK) {
      *buffer = (char *) message->message;
      message->message = nullptr;
      *buflen = message->message_length;
      if (tag) {
         *tag = static_cast<ClientServerBaseTag>(message->message_tag);
      }

      if (sender != nullptr) {
         DEBUG("received from: %s,%lu\n", sender->comp_host, sender->comp_id);
         if (fromcommproc != nullptr && fromcommproc[0] == '\0') {
            strcpy(fromcommproc, sender->comp_name);
         }
         if (fromhost != nullptr) {
            strcpy(fromhost, sender->comp_host);
         }
         if (fromid != nullptr) {
            *fromid = (u_short) sender->comp_id;
         }
      }
   }

   cl_com_free_message(&message);
   cl_com_free_endpoint(&sender);

   DRETURN(ret);
}


/*---------------------------------------------------------
 *  sge_send_any_request
 *  returns 0 if ok
 *          -4 if peer is not alive or rhost == nullptr
 *          return value of gdi_send_message() for other errors
 *
 *  NOTES
 *     MT-NOTE: sge_send_gdi_request() is MT safe (assumptions)
 *
 *     The function does *not* wait until the message is actually sent!
 *---------------------------------------------------------*/
int
ocs::gdi::ClientServerBase::sge_gdi_send_any_request(int synchron, u_long32 *mid, const char *rhost, const char *commproc, int id,
                                                 sge_pack_buffer *pb, ClientServerBaseTag tag, u_long32 response_id, lList **alpp) {
   DENTER(TOP_LAYER);
   int i;
   cl_xml_ack_type_t ack_type = CL_MIH_MAT_NAK;
   cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
   unsigned long dummy_mid = 0;
   unsigned long *mid_pointer = nullptr;
   u_long32 to_port = bootstrap_get_sge_qmaster_port();

   if (rhost == nullptr) {
      answer_list_add(alpp, MSG_GDI_RHOSTISNULLFORSENDREQUEST, STATUS_ESYNTAX,
                      ANSWER_QUALITY_ERROR);
      DRETURN(CL_RETVAL_PARAMS);
   }

   if (handle == nullptr) {
      answer_list_add(alpp, MSG_GDI_NOCOMMHANDLE, STATUS_NOCOMMD, ANSWER_QUALITY_ERROR);
      DRETURN(CL_RETVAL_HANDLE_NOT_FOUND);
   }

   if (strcmp(commproc, (char *) prognames[QMASTER]) == 0 && id == 1) {
      cl_com_append_known_endpoint_from_name((char *) rhost, (char *) commproc, id, (int)to_port, CL_CM_AC_DISABLED, true);
   }

   if (synchron) {
      ack_type = CL_MIH_MAT_ACK;
   }

   if (mid) {
      mid_pointer = &dummy_mid;
   }

   i = cl_commlib_send_message(handle, (char *) rhost, (char *) commproc, id, ack_type, (cl_byte_t **) &pb->head_ptr,
                               (unsigned long) pb->bytes_used, mid_pointer, response_id, tag, false, (bool) synchron);

   if (mid) {
      *mid = dummy_mid;
   }

   if (i != CL_RETVAL_OK) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_SENDMESSAGETOCOMMPROCFAILED_SSISS, (synchron ? "" : "a"),
               commproc, id, rhost, cl_get_error_text(i));
      answer_list_add(alpp, SGE_EVENT, STATUS_NOCOMMD, ANSWER_QUALITY_ERROR);
   }

   DRETURN(i);
}


/*----------------------------------------------------------
 * sge_get_any_request
 *
 * returns 0               on success
 *         -1              rhost is nullptr
 *         commlib return values (always positive)
 *
 * NOTES
 *    MT-NOTE: sge_get_any_request() is MT safe (assumptions)
 *----------------------------------------------------------*/
int
ocs::gdi::ClientServerBase::sge_gdi_get_any_request(char *rhost, char *commproc, u_short *id, sge_pack_buffer *pb,
                                                ClientServerBaseTag *tag, int synchron, u_long32 for_request_mid, u_long32 *mid) {
   DENTER(GDI_LAYER);
   int i;
   ushort usid = 0;
   cl_com_message_t *message = nullptr;
   cl_com_endpoint_t *sender = nullptr;

   PROF_START_MEASUREMENT(SGE_PROF_GDI);

   if (id) {
      usid = (ushort) *id;
   }

   if (!rhost) {
      ERROR(SFNMAX, MSG_GDI_RHOSTISNULLFORGETANYREQUEST);
      PROF_STOP_MEASUREMENT(SGE_PROF_GDI);
      DRETURN(-1);
   }

   cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);

   /* TODO: do trigger or not? depends on synchron
    * TODO: Remove synchron flag from this function, it is only used for get_event_list call in event client.
            event client code should be re-written, not to use this synchron flag set to false
    */
   if (synchron == 0) {
      cl_commlib_trigger(handle, 0);
   }

   i = cl_commlib_receive_message(handle, rhost, commproc, usid, (bool) synchron, for_request_mid, &message, &sender);

   if (i == CL_RETVAL_CONNECTION_NOT_FOUND) {
      if (commproc[0] != '\0' && rhost[0] != '\0') {
         /* The connection was closed, reopen it */
         i = cl_commlib_open_connection(handle, (char *) rhost, (char *) commproc, usid);
         INFO("reopen connection to %s,%s," sge_uu32 " (2)\n", rhost, commproc, usid);
         if (i == CL_RETVAL_OK) {
            INFO("reconnected successfully\n");
            i = cl_commlib_receive_message(handle, rhost, commproc, usid,
                                           (bool) synchron, for_request_mid,
                                           &message, &sender);
         }
      } else {
         DEBUG("can't reopen a connection to unspecified host or commproc (2)\n");
      }
   }

   if (i != CL_RETVAL_OK) {
      if (i != CL_RETVAL_NO_MESSAGE) {
         /* This if for errors */
         DPRINTF(SGE_EVENT, MSG_GDI_RECEIVEMESSAGEFROMCOMMPROCFAILED_SISS, (commproc[0] ? commproc : "any"), (int) usid, (commproc[0] ? commproc : "any"), cl_get_error_text(i));
      }
      cl_com_free_message(&message);
      cl_com_free_endpoint(&sender);
      /* This is if no message is there */
      PROF_STOP_MEASUREMENT(SGE_PROF_GDI);
      DRETURN(i);
   }

   /* ok, we received a message */
   if (message != nullptr) {
      /* TODO: there are two cases for any and addressed communication partner, two functions are needed */
      if (sender != nullptr && id) {
         *id = (u_short) sender->comp_id;
      }
      if (tag) {
         *tag = static_cast<ClientServerBaseTag>(message->message_tag);
      }
      if (mid) {
         *mid = message->message_id;
      }


      /* fill it in the packing buffer */
      i = init_packbuffer_from_buffer(pb, (char *) message->message, message->message_length);

      /* TODO: the packbuffer must be hold, not deleted !!! */
      message->message = nullptr;

      if (i != PACK_SUCCESS) {
         ERROR(MSG_GDI_ERRORUNPACKINGGDIREQUEST_S, cull_pack_strerror(i));
         PROF_STOP_MEASUREMENT(SGE_PROF_GDI);
         DRETURN(CL_RETVAL_READ_ERROR);
      }

      /* TODO: there are two cases for any and addressed communication partner, two functions are needed */
      if (sender != nullptr) {
         DEBUG("received from: %s,%ld\n", sender->comp_host, sender->comp_id);
         if (rhost[0] == '\0') {
            strcpy(rhost, sender->comp_host); /* If we receive from anybody return the sender */
         }
         if (commproc[0] == '\0') {
            strcpy(commproc, sender->comp_name); /* If we receive from anybody return the sender */
         }
      }

      cl_com_free_endpoint(&sender);
      cl_com_free_message(&message);
   }
   PROF_STOP_MEASUREMENT(SGE_PROF_GDI);
   DRETURN(CL_RETVAL_OK);
}

const char *
ocs::gdi::ClientServerBase::to_string(unsigned long tag_value) {
   auto tag = static_cast<ClientServerBaseTag>(tag_value);

   switch (tag) {
      case TAG_NONE: return "TAG_NONE";
      case TAG_GDI_REQUEST: return "TAG_GDI_REQUEST";
      case TAG_ACK_REQUEST: return "TAG_ACK_REQUEST";
      case TAG_REPORT_REQUEST: return "TAG_REPORT_REQUEST";
      case TAG_JOB_EXECUTION: return "TAG_JOB_EXECUTION";
      case TAG_SLAVE_ALLOW: return "TAG_SLAVE_ALLOW";
      case TAG_CHANGE_TICKET: return "TAG_CHANGE_TICKET";
      case TAG_SIGJOB: return "TAG_SIGJOB";
      case TAG_SIGQUEUE: return "TAG_SIGQUEUE";
      case TAG_KILL_EXECD: return "TAG_KILL_EXECD";
      case TAG_GET_NEW_CONF: return "TAG_GET_NEW_CONF";
      case TAG_TASK_EXIT: return "TAG_TASK_EXIT";
      case TAG_FULL_LOAD_REPORT: return "TAG_FULL_LOAD_REPORT";
      case TAG_EVENT_CLIENT_EXIT: return "TAG_EVENT_CLIENT_EXIT";
      default:
         break;
   }
   return "TAG_NOT_DEFINED";
}
