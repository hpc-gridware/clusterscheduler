#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include <vector>

#include "basis_types.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_tq.h"

#include "comm/cl_communication.h"

#include "gdi/ocs_GdiTask.h"

extern sge_tq_queue_t *GlobalRequestQueue;
extern sge_tq_queue_t *ReaderRequestQueue;
extern sge_tq_queue_t *ReaderWaitingRequestQueue;

typedef enum {
   PACKET_GDI_REQUEST,
   PACKET_REPORT_REQUEST,
   PACKET_ACK_REQUEST
} gdi_packet_request_type_t;

namespace ocs {
   // request types that can be encapsulated into packages/tasks


   class GdiPacket {
   public:
      /*
       * mutex to gard the "is_handled" flag of this structure
       */
      pthread_mutex_t mutex;

      /*
       * condition used for synchronisation of multiple threads
       * which want to access this structure
       */
      pthread_cond_t cond;

   private:
      /*
       * true if the worker thread does not need to access this
       * structure anymore. Guarded with "mutex" part of this
       * structure
       */
      bool is_handled;

   public:
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
       * EB: TODO: Cleanup: remove "auth_info" from ocs::GdiPacket
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
       * EB: TODO: Cleanup: eleminate "pb" from ocs::GdiPacket
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

      GdiPacket();
      ~GdiPacket();

      int append_task(lList **answer_list, GdiTarget::Target target, u_long32 command,
                      lList **lp, lList **a_list, lCondition **condition, lEnumeration **enumeration, bool do_copy);
      bool initialize_auth_info();
      bool parse_auth_info(lList **answer_list, uid_t *uid, char *user, size_t user_len, gid_t *gid, char *group, size_t group_len, int *amount, ocs_grp_elem_t **grp_array);
      void create_multi_answer(lList **malpp);
      void wait_till_handled();
      bool get_is_handled();
      void broadcast_that_handled();
      bool execute_external(lList **answer_list);
      bool execute_internal(lList **answer_list);
      void wait_for_result_external(lList **malpp);
      void wait_for_result_internal(lList **malpp);

      u_long32 get_pb_size();
      bool unpack(lList **answer_list, sge_pack_buffer *pb);
      bool pack(lList **answer_list, sge_pack_buffer *pb);
      bool pack_task(GdiTask *task, lList **answer_list, sge_pack_buffer *pb, bool has_next);
      void debug_print();
   };
}