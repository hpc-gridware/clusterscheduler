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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"

#include "gdi/ocs_gdi_Packet.h"

namespace ocs::gdi {
   class ClientServerBase {
      static int gdi_send_message(int synchron, const char *tocomproc, int toid, const char *tohost, int tag, char **buffer, int buflen, u_long32 *mid);

   public:
      enum ClientServerBaseTag {
         TAG_NONE = 0,     /* usable e.g. as delimiter in a tag array */
         TAG_GDI_REQUEST,
         TAG_ACK_REQUEST,
         TAG_REPORT_REQUEST,
         TAG_JOB_EXECUTION,
         TAG_SLAVE_ALLOW,
         TAG_CHANGE_TICKET,
         TAG_SIGJOB,
         TAG_SIGQUEUE,
         TAG_KILL_EXECD,
         TAG_GET_NEW_CONF,
         TAG_TASK_EXIT,
         TAG_EVENT_CLIENT_EXIT,
         TAG_FULL_LOAD_REPORT
      #ifdef KERBEROS
         ,TAG_AUTH_FAILURE
      #endif
      };

      // @todo: is this really required?
      typedef struct {
         char snd_host[CL_MAXHOSTNAMELEN]; /* sender hostname; nullptr -> all              */
         char snd_name[CL_MAXHOSTNAMELEN]; /* sender name (aka 'commproc'); nullptr -> all */
         u_short snd_id;            /* sender identifier; 0 -> all               */
         ClientServerBaseTag tag;                   /* message tag; TAG_NONE -> all              */
         u_long32 request_mid;      /* message id of request                     */
         sge_pack_buffer buf;       /* message buffer                            */
      } struct_msg_t;


      // @todo: tag should be of type ClientServerBaseTag
      static const char *to_string(unsigned long tag);

      static int gdi_send_message_pb(int synchron, const char *tocomproc, int toid, const char *tohost, ClientServerBaseTag tag,
                                     sge_pack_buffer *pb, u_long32 *mid);

      static int gdi_receive_message(char *fromcommproc, u_short *fromid, char *fromhost,
                                     ClientServerBaseTag *tag, char **buffer, u_long32 *buflen, int synchron);

      static int sge_gdi_get_any_request(char *rhost, char *commproc, u_short *id, sge_pack_buffer *pb, ClientServerBaseTag *tag,
                                         int synchron, u_long32 for_request_mid, u_long32 *mid);

      static int sge_gdi_send_any_request(int synchron, u_long32 *mid, const char *rhost, const char *commproc, int id,
                                          sge_pack_buffer *pb, ClientServerBaseTag tag, u_long32 response_id, lList **alpp);
      static bool sge_gdi_reresolve_check_user(sge_pack_buffer *pb, bool local_uid_gid, bool reresolve_user,
                                               bool reresolve_supp_grp);
   };
}
