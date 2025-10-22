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
 *  Portions of this software are Copyright (c) 2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_profiling.h"
#include "uti/sge_hostname.h"

#include "gdi/msg_gdilib.h"
#include "gdi/sge_gdi_data.h"

#include "sgeobj/sge_answer.h"

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
      int execd_port = sge_get_execd_port();
      /* we have to send a message to another component than qmaster */
      DEBUG("search handle to \"%s\"\n", tocomproc);
      handle = cl_com_get_handle("execd_handle", 0);
      if (handle == nullptr) {
         int commlib_error = CL_RETVAL_OK;
         cl_framework_t communication_framework = CL_CT_TCP;
         DEBUG("creating handle to \"%s\"\n", tocomproc);
         if (bootstrap_has_security_mode(BS_SEC_MODE_CSP)) {
            DPRINTF("using communication lib with SSL framework (execd_handle)\n");
            communication_framework = CL_CT_SSL;
         } else if (bootstrap_has_security_mode(BS_SEC_MODE_TLS)) {
#if defined(OCS_WITH_OPENSSL)
            DPRINTF("using communication lib with TLS framework (execd_handle)\n");
            communication_framework = CL_CT_SSL_TLS;
            lList *answer_list = nullptr;
            commlib_error = gdi_setup_tls_config(true, false, &answer_list,
                                                 component_get_qualified_hostname(), 0,
                                                 tohost, execd_port, tocomproc);
            if (commlib_error != CL_RETVAL_OK) {
               answer_list_output(&answer_list);
               DRETURN(commlib_error);
            }
#else
            ERROR(SFNMAX, MSG_SSL_NOT_BUILT_IN);
#endif
         }
         cl_com_create_handle(&commlib_error, communication_framework, CL_CM_CT_MESSAGE,
                              false, execd_port, CL_TCP_DEFAULT,
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
   if (pb == nullptr) {
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
         if (bootstrap_has_security_mode(BS_SEC_MODE_CSP)) {
            DPRINTF("using communication lib with SSL framework (execd_handle)\n");
            communication_framework = CL_CT_SSL;
         } else if (bootstrap_has_security_mode(BS_SEC_MODE_TLS)) {
#if defined (OCS_WITH_OPENSSL)
            DPRINTF("using communication lib with SSL framework (execd_handle)\n");
            communication_framework = CL_CT_SSL_TLS;
#else
            ERROR(SFNMAX, MSG_SSL_NOT_BUILT_IN);
#endif
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
         INFO("reopen connection to %s,%s," sge_u32 " (1)", fromhost, fromcommproc, static_cast<u_long32>(*fromid));
         if (ret == CL_RETVAL_OK) {
            INFO("reconnected successfully");
            ret = cl_commlib_receive_message(handle, fromhost, fromcommproc, *fromid, (bool) synchron, 0, &message,
                                             &sender);
         }
      } else {
         DEBUG("can't reopen a connection to unspecified host or commproc (1)");
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
         INFO("reopen connection to %s,%s,%d (2)", rhost, commproc, (int)usid);
         if (i == CL_RETVAL_OK) {
            INFO("reconnected successfully");
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
         ERROR(MSG_GDI_ERRORUNPACKINGGDIREQUEST_SSUS, sender->comp_host, sender->comp_name, static_cast<u_long32>(sender->comp_id), cull_pack_strerror(i));
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

/**
 * @brief re-resolve and check user information
 *
 * Calls cull_reresolve_check_user() and reacts to the result:
 * If an error is reported this will be output as ERROR message.
 * If no error is reported but the dstring passed into cull_reresolve_check_user() contains a message
 * then it is output as INFO message.
 *
 * @param pb the pack buffer containing the request and usage data
 * @param local_uid_gid only accept requests with the same uid and gid?
 * @param reresolve_user  re-resolve and correct the user and group name?
 * @param reresolve_supp_grp  re-resolve the supplementary group ids?
 * @return true if all was ok, possibly names were corrected, false on errors
 */
bool
ocs::gdi::ClientServerBase::sge_gdi_reresolve_check_user(sge_pack_buffer *pb, bool local_uid_gid, bool reresolve_user,
                                                         bool reresolve_supp_grp) {
   DENTER(TOP_LAYER);
   bool ret;
   DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);

   ret = cull_reresolve_check_user(pb, &error_dstr, local_uid_gid, reresolve_user, reresolve_supp_grp);
   if (!ret) {
      ERROR(SFNMAX, sge_dstring_get_string(&error_dstr));
   } else if (sge_dstring_strlen(&error_dstr) > 0) {
      INFO(SFNMAX, sge_dstring_get_string(&error_dstr));
   }

   DRETURN(ret);
}

#if defined(OCS_WITH_OPENSSL)
int
ocs::gdi::ClientServerBase::gdi_setup_tls_config(bool needs_client, bool is_server, lList **answer_list,
                                                 const char *local_host, u_long32 local_port,
                                                 const char *target_host, u_long32 target_port, const char *target_commproc) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(dstr_error, MAX_STRING_SIZE);
   if (!ocs::uti::OpenSSL::is_openssl_available() && !ocs::uti::OpenSSL::initialize(&dstr_error)) {
      DPRINTF("initializing OpenSSL failed: %s\n", sge_dstring_get_string(&dstr_error));
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "initializing OpenSSL failed: %s", sge_dstring_get_string(&dstr_error)); // @todo i18n
      DRETURN(CL_RETVAL_UNKNOWN);
   }

   // set up a ssl_config to pass cert and key path to commlib
   // we must pass different certs to commlib:
   // - if we are server: use our own cert and key
   // - if we are client: use qmaster cert to verify qmaster / execd cert to verify execd in case of qrsh -inherit
   // - we might need both, e.g. for sge_execd
   // - and what if the qmaster name changes? Then we need to update the client cert.
   std::string client_cert_path;
   std::string server_cert_path;
   std::string server_key_path;
   ocs::uti::OpenSSL::build_cert_path(client_cert_path, nullptr, target_host, target_commproc);
   if (is_server) {
      const char *comp_name = component_get_component_name();
      ocs::uti::OpenSSL::build_cert_path(server_cert_path, nullptr, local_host, comp_name);
      ocs::uti::OpenSSL::build_key_path(server_key_path, nullptr, local_host, local_port, comp_name);
   }
   cl_ssl_setup_t *sec_ssl_setup_config = nullptr;
   int cl_ret = cl_com_create_ssl_setup(&sec_ssl_setup_config, CL_SSL_PEM_FILE, CL_SSL_TLS,
                                    client_cert_path.c_str(),
                                    server_cert_path.c_str(),
                                    server_key_path.c_str(),
                                    false, needs_client);
   if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
      DPRINTF("return value of cl_com_create_ssl_setup(): %s\n", cl_get_error_text(cl_ret));
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_GDI_CANT_CONNECT_HANDLE_SSUUS, local_host, component_get_component_name(),
                              0, target_port, cl_get_error_text(cl_ret));
      cl_com_free_ssl_setup(&sec_ssl_setup_config);
      DRETURN(cl_ret);
   }
   cl_ret = cl_com_specify_ssl_configuration(sec_ssl_setup_config);
   cl_com_free_ssl_setup(&sec_ssl_setup_config);
   if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
      DPRINTF("return value of cl_com_specify_ssl_configuration(): %s\n", cl_get_error_text(cl_ret));
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_GDI_CANT_CONNECT_HANDLE_SSUUS, local_host, component_get_component_name(),
                              0, target_port, cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   return CL_RETVAL_OK;
}

int
ocs::gdi::ClientServerBase::gdi_update_client_tls_config(lList **answer_list, const char *master_host) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(dstr_error, MAX_STRING_SIZE);
   // openssl must already have been initialized earlier
   if (!ocs::uti::OpenSSL::is_openssl_available()) {
      DPRINTF("OpenSSL is not initialized: %s\n", sge_dstring_get_string(&dstr_error));
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "OpenSSL is not initialized: %s", sge_dstring_get_string(&dstr_error)); // @todo i18n
      DRETURN(CL_RETVAL_UNKNOWN); // @todo add new commlib error code
   }

   // set up a ssl_config to pass cert and key path to commlib
   // we must pass different certs to commlib:
   // - if we are server: use our own cert and key
   // - if we are client: use qmaster cert to verify qmaster
   // - we might need both, e.g. for sge_execd
   // - and what if the qmaster name changes? Then we need to update the client cert.
   std::string client_cert_path;
   ocs::uti::OpenSSL::build_cert_path(client_cert_path, nullptr, master_host, prognames[QMASTER]);
   cl_ssl_setup_t *sec_ssl_setup_config = nullptr;
   int cl_ret = cl_com_create_ssl_setup(&sec_ssl_setup_config, CL_SSL_PEM_FILE, CL_SSL_TLS,
                                    client_cert_path.c_str(),
                                    nullptr, nullptr, true);
   if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
      DPRINTF("return value of cl_com_create_ssl_setup(): %s\n", cl_get_error_text(cl_ret));
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_GDI_CANT_CONNECT_HANDLE_SSUUS, "localhost", component_get_component_name(),
                              0, 0, cl_get_error_text(cl_ret));
      cl_com_free_ssl_setup(&sec_ssl_setup_config);
      DRETURN(cl_ret);
   }
   cl_ret = cl_com_update_ssl_configuration(sec_ssl_setup_config);
   cl_com_free_ssl_setup(&sec_ssl_setup_config);
   if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
      DPRINTF("return value of cl_com_update_ssl_configuration(): %s\n", cl_get_error_text(cl_ret));
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_GDI_CANT_CONNECT_HANDLE_SSUUS, "localhost", component_get_component_name(),
                              0, 0, cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
   cl_ret = cl_commlib_handle_update_ssl_client_context(handle);
   if (cl_ret != CL_RETVAL_OK && cl_ret != gdi_data_get_last_commlib_error()) {
      DPRINTF("return value of cl_com_update_ssl_configuration(): %s\n", cl_get_error_text(cl_ret));
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_GDI_CANT_CONNECT_HANDLE_SSUUS, "localhost", component_get_component_name(),
                              0, 0, cl_get_error_text(cl_ret));
      DRETURN(cl_ret);
   }

   return CL_RETVAL_OK;
}
#endif
