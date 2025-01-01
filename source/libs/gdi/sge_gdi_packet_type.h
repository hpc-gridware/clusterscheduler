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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <vector>

#include "basis_types.h"

#include "uti/sge_uidgid.h"

#include "gdi/ocs_GdiMulti.h"
#include "gdi/ocs_GdiTask.h"

#include "cull/cull.h"

#include "comm/cl_communication.h"

// request types that can be encapsulated into packages/tasks
typedef enum {
   PACKET_GDI_REQUEST,
   PACKET_REPORT_REQUEST,
   PACKET_ACK_REQUEST
} gdi_packet_request_type_t;

struct sge_gdi_packet_class_t {
   /* 
    * mutex to gard the "is_handled" flag of this structure 
    */
   pthread_mutex_t mutex;

   /*
    * condition used for synchronisation of multiple threads
    * which want to access this structure
    */
   pthread_cond_t cond;

   /*
    * true if the worker thread does not need to access this
    * structure anymore. Guarded with "mutex" part of this 
    * structure
    */
   bool is_handled;

   /*
    * true if this structure was created by a qmaster
    * internal thread (scheduler, JVM...)
    */
   bool is_intern_request;

   // request_type GID/Report/ACK/...
   gdi_packet_request_type_t request_type;

   /*
    * unique id identifying this packet uniquely in the context
    * of the creating process/thread
    */
   u_long32 id;

   /*
    * set in qmaster to identify the source for this GDI packet.
    * qmaster will use that information to send a response
    * to the correct external communication partner using the
    * commlib infrastructure
    */
   char host[CL_MAXHOSTNAMELEN];
   char commproc[CL_MAXHOSTNAMELEN];
   u_short commproc_id;
   u_long32 response_id;
   u_long64 gdi_session;

   /*
    * GDI version of this structure
    */
   u_long32 version;

   /*
    * pointers to the first and last task part of a multi
    * GDI request. This list contains at least one element
    */
   std::vector<ocs::GdiTask *> tasks;

   /*
    * encrypted authenitication information. This information will 
    * be decrypted to the field "uid", "gid", "user" and "group"
    * part of this structure 
    *
    * EB: TODO: Cleanup: remove "auth_info" from sge_gdi_packet_class_t
    *
    *    authinfo is not needed in this structure because the
    *    same information is stored in "uid", "gid", "user" and "group"
    *    If these elements are initialized during unpacking a packet
    *    and if the GDI functions don't want to access auth_info
    *    anymore then we can remove that field from this structure.
    */
   char *auth_info;  

   /*
    * User/group information of that user which used GDI functionality.
    * Used in qmasters GDI handling code to identify if that
    * user has the allowance to do the requested GDI activities. 
    */
   uid_t uid;
   gid_t gid;
   char user[128];
   char group[128];
   int amount;
   ocs_grp_elem_t *grp_array;

   /*
    * Packbuffer used for GDI GET requests to directly store the 
    * result of lSelectHashPack()
    *
    * EB: TODO: Cleanup: eleminate "pb" from sge_gdi_packet_class_t
    *    
    *    We might eleminate this member as soon as pure GDI GET
    *    requests are handled by some kind of read only thread.
    *    in qmaster. Write requests don't need the packbuffer.
    *    Due to that fact we could create and release the packbuffer
    *    in the the listener thread and use cull lists (part
    *    of the task sublist) to transfer GDI result information
    *    from the worker to the listener then we are able to
    *    remove pb. 
    */
   sge_pack_buffer pb;

   // DS hint
   u_long32 ds_type;
};
