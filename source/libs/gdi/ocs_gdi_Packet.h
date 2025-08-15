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

#include "gdi/ocs_gdi_Task.h"

extern sge_tq_queue_t *GlobalRequestQueue;
extern sge_tq_queue_t *ReaderRequestQueue;
extern sge_tq_queue_t *ReaderWaitingRequestQueue;

typedef enum {
   PACKET_GDI_REQUEST,
   PACKET_REPORT_REQUEST,
   PACKET_ACK_REQUEST
} gdi_packet_request_type_t;

namespace ocs::gdi {
   void
   destroy_task_queues();

   // request types that can be encapsulated into packages/tasks

   class Packet {
   public:
      pthread_mutex_t mutex;
      pthread_cond_t cond;

   private:
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
      std::vector<ocs::gdi::Task *> tasks;

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
       * EB: TODO: Cleanup: eleminate "pb" from ocs::gdi::Packet
       *
       *    We might eliminate this member as soon as pure GDI GET
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

      Packet();
      ~Packet();

      int append_task(gdi::Task *task);

      void create_multi_answer(lList **malpp);

      bool get_is_handled();
      void wait_till_handled();
      void broadcast_that_handled();

      bool execute_internal(lList **answer_list);
      void wait_for_result_internal(lList **malpp);

      bool execute_external(lList **answer_list);
      void wait_for_result_external(lList **malpp);

      u_long32 get_pb_size();
      bool unpack(lList **answer_list, sge_pack_buffer *pb);
      bool unpack_header(lList **answer_list, sge_pack_buffer *pb);

      bool pack(lList **answer_list, sge_pack_buffer *pb);
      bool pack_header(lList **answer_list, sge_pack_buffer *pb);
      bool pack_task(gdi::Task *task, lList **answer_list, sge_pack_buffer *pb, bool has_next);
      void debug_print();
   };
}
