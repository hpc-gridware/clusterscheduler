#pragma once
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
 *  Portions of this software are Copyright (c) 2025-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Message exchange shared by GDI client and server
 */

#include "cull/cull.h"

#include "gdi/ocs_gdi_Packet.h"

namespace ocs::gdi {
   /**
    * @brief The message layer shared by GDI clients and qmaster
    *
    * Wraps the commlib: every message carries a @ref ClientServerBaseTag
    * saying what it is, and the receiver dispatches on it — see
    * `daemons/execd/dispatcher.cc` for the execd side.
    */
   class ClientServerBase {
      static int gdi_send_message(int synchron, const char *tocomproc, int toid, const char *tohost, int tag, char **buffer, int buflen, uint32_t *mid);

   public:
      /// What a message contains, and therefore who handles it
      enum ClientServerBaseTag {
         TAG_NONE = 0,            ///< no tag; usable e.g. as delimiter in a tag array
         TAG_GDI_REQUEST,         ///< a GDI request or its answer
         TAG_ACK_REQUEST,         ///< an acknowledge carrying no payload
         TAG_REPORT_REQUEST,      ///< a load or job report from an execution daemon
         TAG_JOB_EXECUTION,       ///< qmaster to execd: start this job
         TAG_SLAVE_ALLOW,         ///< qmaster to execd: this host may run slave tasks of a parallel job
         TAG_CHANGE_TICKET,       ///< qmaster to execd: new ticket amounts for running jobs
         TAG_SIGJOB,              ///< qmaster to execd: signal a job
         TAG_SIGQUEUE,            ///< qmaster to execd: signal every job of a queue
         TAG_KILL_EXECD,          ///< qmaster to execd: shut down
         TAG_GET_NEW_CONF,        ///< qmaster to execd: re-read your configuration
         TAG_TASK_EXIT,           ///< execd to qmaster: a task of a parallel job ended
         TAG_EVENT_CLIENT_EXIT,   ///< an event client is going away
         TAG_FULL_LOAD_REPORT,    ///< qmaster to execd: send a complete load report, not a delta
         TAG_RECONNECT_PREPARE          ///< qmaster → execd: write reconnect.info into a job's active_jobs spool (CS-2143)
      #ifdef KERBEROS
         ,TAG_AUTH_FAILURE     ///< Kerberos authentication failed
      #endif
      };

      /**
       * @brief One received message, together with who sent it
       *
       * When used to *select* which message to receive, an empty or zero field
       * means "any": an empty #snd_host matches every host, #TAG_NONE every tag.
       *
       * @todo is this really required?
       */
      typedef struct {
         char snd_host[CL_MAXHOSTNAMELEN]; ///< sender host name; empty matches any
         char snd_name[CL_MAXHOSTNAMELEN]; ///< sender component name, aka `commproc`; empty matches any
         u_short snd_id;                   ///< sender identifier; 0 matches any
         ClientServerBaseTag tag;          ///< what the message contains; #TAG_NONE matches any
         uint32_t request_mid;             ///< id of the request this answers
         sge_pack_buffer buf;              ///< the message payload
      } struct_msg_t;


      static const char *to_string(unsigned long tag);

      static int gdi_send_message_pb(int synchron, const char *tocomproc, int toid, const char *tohost, ClientServerBaseTag tag,
                                     sge_pack_buffer *pb, uint32_t *mid);

      static int gdi_receive_message(char *fromcommproc, u_short *fromid, char *fromhost,
                                     ClientServerBaseTag *tag, char **buffer, uint32_t *buflen, int synchron);

      static int sge_gdi_get_any_request(char *rhost, char *commproc, u_short *id, sge_pack_buffer *pb, ClientServerBaseTag *tag,
                                         int synchron, uint32_t for_request_mid, uint32_t *mid);

      static int sge_gdi_send_any_request(int synchron, uint32_t *mid, const char *rhost, const char *commproc, int id,
                                          sge_pack_buffer *pb, ClientServerBaseTag tag, uint32_t response_id, lList **alpp);
      static bool sge_gdi_reresolve_check_user(sge_pack_buffer *pb, bool local_uid_gid, bool reresolve_user,
                                               bool reresolve_supp_grp);
#if defined(OCS_WITH_OPENSSL)
      static int gdi_setup_tls_config(bool needs_client, bool is_server, lList **answer_list,
                                      const char *local_host, uint32_t local_port, const char *target_host, uint32_t target_port, const char *target_commproc);
      static int gdi_update_client_tls_config(lList **answer_list, const char *master_host);
#endif
   };
}
