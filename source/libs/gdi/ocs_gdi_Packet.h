#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025-2026 HPC-Gridware GmbH
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

/** @file
 * @brief The wire format a GDI request travels in
 */

#include <vector>

#include <cinttypes>
#include "uti/sge_uidgid.h"
#include "uti/sge_tq.h"

#include "comm/cl_communication.h"

#include "gdi/ocs_gdi_Task.h"

/// Queue the worker threads take GDI packets from
extern sge_tq_queue_t *GlobalRequestQueue;
/// Queue the reader threads take read-only GDI packets from
extern sge_tq_queue_t *ReaderRequestQueue;
/// Packets waiting for the reader data store to catch up before they may run
extern sge_tq_queue_t *ReaderWaitingRequestQueue;

/// What kind of message a packet carries
typedef enum {
   PACKET_GDI_REQUEST,    ///< a GDI request, the usual case
   PACKET_REPORT_REQUEST, ///< a load or job report from an execution daemon
   PACKET_ACK_REQUEST     ///< an acknowledge, carrying no payload
} gdi_packet_request_type_t;

namespace ocs::gdi {
   /// Release the request queues at shutdown
   void
   destroy_task_queues();

   // request types that can be encapsulated into packages/tasks

   /**
    * @brief One GDI message on the wire, holding a list of @ref Task
    *
    * A packet is what actually travels between a client and qmaster. It
    * carries the tasks, the sender's identity, and enough addressing for
    * qmaster to send the answer back to the right communication partner.
    *
    * Two paths exist. #execute_external packs the packet, sends it through the
    * commlib and waits for a reply. #execute_internal is used by qmaster's own
    * threads: nothing is packed or sent, the packet is simply put on
    * #GlobalRequestQueue and the caller waits on #cond until a worker signals
    * it through #broadcast_that_handled.
    */
   class Packet {
   public:
      pthread_mutex_t mutex; ///< guards #is_handled and #cond
      pthread_cond_t cond;   ///< signalled once the packet has been handled

   private:
      bool is_handled;       ///< true once a worker finished this packet

   public:
      /*
       * true if this structure was created by a qmaster
       * internal thread (scheduler, JVM...)
       */
      bool is_intern_request;                        ///< true when the packet was created by a qmaster internal thread, e.g. the scheduler

      // request_type GID/Report/ACK/...
      gdi_packet_request_type_t request_type;        ///< which kind of message this packet carries

      /*
       * set in qmaster to identify the source for this GDI packet.
       * qmaster will use that information to send a response
       * to the correct external communication partner using the
       * commlib infrastructure
       */
      char host[CL_MAXHOSTNAMELEN];                  ///< host the request came from, so the answer reaches it
      char commproc[CL_MAXHOSTNAMELEN];              ///< name of the sending component, e.g. `qconf`
      u_short commproc_id;                           ///< commlib id of the sending component
      uint32_t response_id;                          ///< id the answer must carry so the sender can match it
      uint64_t gdi_session;                          ///< session this request belongs to, for read-after-write consistency

      /*
       * GDI version of this structure
       */
      uint32_t version;                              ///< GDI version of the sender, checked on unpack

      /*
       * pointers to the first and last task part of a multi
       * GDI request. This list contains at least one element
       */
      std::vector<Task *> tasks;                     ///< the operations this packet carries; always at least one

      /*
       * User/group information of that user which used GDI functionality.
       * Used in qmasters GDI handling code to identify if that
       * user has the allowance to do the requested GDI activities.
       */
      uid_t uid;                                     ///< user id of the caller, used for the permission check in qmaster
      gid_t gid;                                     ///< primary group id of the caller
      char user[128];                                ///< user name of the caller
      char group[128];                               ///< primary group name of the caller
      int amount;                                    ///< number of supplementary groups in #grp_array
      ocs_grp_elem_t *grp_array;                     ///< supplementary groups of the caller

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
      /**
       * @brief Buffer a GET request's result is packed into directly
       *
       * Lets `lSelectHashPack()` write straight into the outgoing buffer
       * instead of building an intermediate list; see
       * Task::do_select_pack_simultaneous.
       *
       * @todo eliminate "pb" from ocs::gdi::Packet. It could go once pure GDI
       *       GET requests are handled by a read-only thread in qmaster: write
       *       requests do not need it, so the listener thread could create and
       *       release the buffer and use cull lists to carry the result from
       *       the worker back to the listener.
       */
      sge_pack_buffer pb;

      // DS hint
      uint32_t ds_type;                              ///< which data store, and therefore which thread type, should execute this packet

      Packet();
      ~Packet();

      /**
       * @brief Add a task to this packet
       * @param task the task to append; the packet takes it over
       * @return the task's index, used to pick its answer out later
       */
      int append_task(Task *task);

      /**
       * @brief Build the multi answer list from the tasks' individual answers
       * @param[out] malpp receives one entry per task
       */
      void create_multi_answer(lList **malpp);

      /// Has a worker finished this packet?
      bool get_is_handled();
      /// Block until a worker signals that this packet is finished
      void wait_till_handled();
      /// Mark this packet finished and wake everyone waiting on it
      void broadcast_that_handled();

      /**
       * @brief Hand this packet to a worker thread inside qmaster
       *
       * Fills in the caller's identity from the component, then puts the
       * packet on #GlobalRequestQueue. Nothing is packed or sent — this is the
       * path qmaster's own threads take.
       *
       * @param[out] answer_list receives errors detected while queueing
       * @return true when the packet was queued
       */
      bool execute_internal(lList **answer_list);
      /**
       * @brief Wait for a packet queued by #execute_internal and take its answers
       * @param[out] malpp receives the multi answer list
       */
      void wait_for_result_internal(lList **malpp);

      /**
       * @brief Pack this packet and send it to qmaster through the commlib
       *
       * The path an ordinary client takes.
       *
       * @param[out] answer_list receives packing or communication errors
       * @return true when the packet was sent
       */
      bool execute_external(lList **answer_list);
      /**
       * @brief Wait for the reply to a packet sent by #execute_external
       * @param[out] malpp receives the multi answer list
       */
      void wait_for_result_external(lList **malpp);

      /**
       * @brief How large a packbuffer this packet needs
       * @return the size in bytes, measured by packing in counting mode
       */
      uint32_t get_pb_size();
      /**
       * @brief Read a whole packet, header and tasks, out of a packbuffer
       * @param[out] answer_list receives the reason on failure
       * @param pb the buffer to read from
       * @return true on success
       */
      bool unpack(lList **answer_list, sge_pack_buffer *pb);
      /**
       * @brief Read only the header, so the receiver can decide who handles it
       * @param[out] answer_list receives the reason on failure
       * @param pb the buffer to read from
       * @return true on success
       */
      bool unpack_header(lList **answer_list, sge_pack_buffer *pb);

      /**
       * @brief Write a whole packet, header and tasks, into a packbuffer
       * @param[out] answer_list receives the reason on failure
       * @param pb the buffer to append to
       * @return true on success
       */
      bool pack(lList **answer_list, sge_pack_buffer *pb);
      /**
       * @brief Write only the header into a packbuffer
       * @param[out] answer_list receives the reason on failure
       * @param pb the buffer to append to
       * @return true on success
       */
      bool pack_header(lList **answer_list, sge_pack_buffer *pb);
      /**
       * @brief Write one task into a packbuffer
       * @param task the task to write
       * @param[out] answer_list receives the reason on failure
       * @param pb the buffer to append to
       * @param has_next true when another task follows, so the reader keeps going
       * @return true on success
       */
      bool pack_task(Task *task, lList **answer_list, sge_pack_buffer *pb, bool has_next);
      /// Log this packet's header and tasks at debug level
      void debug_print();
   };
}
