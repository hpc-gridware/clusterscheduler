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

#include <pthread.h>

#include "basis_types.h"

#include "uti/ocs_Munge.h"
#include "uti/ocs_cond.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_security.h"

#include "sgeobj/ocs_Version.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_jsv.h"

#include "gdi/ocs_gdi_ClientBase.h"
#include "gdi/sge_gdi_data.h"
#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Task.h"

#include "msg_gdilib.h"
#include "msg_qmaster.h"
#include "msg_common.h"

#include <ocs_gdi_ClientServerBase.h>
#include <uti/ocs_cond.h>

#define CLIENT_WAIT_TIME_S 1
#define GDI_PACKET_MUTEX "gdi_packet_mutex"

sge_tq_queue_t *GlobalRequestQueue = nullptr;
sge_tq_queue_t *ReaderRequestQueue = nullptr;
sge_tq_queue_t *ReaderWaitingRequestQueue = nullptr;

static bool get_cl_ping_value() {
   char* cl_ping = nullptr;
   bool do_ping = false;

   cl_com_get_parameter_list_value("cl_ping", &cl_ping);
   if (cl_ping != nullptr) {
      if (strcasecmp(cl_ping, "true") == 0) {
         do_ping = true;
      }
      sge_free(&cl_ping);
   }
   return do_ping;
}

static int get_gdi_retries_value() {
   char* gdi_retries = nullptr;
   int retries = 0;
   cl_com_get_parameter_list_value("gdi_retries", &gdi_retries);
   if (gdi_retries != nullptr) {
      retries = atoi(gdi_retries);
      sge_free(&gdi_retries);
   }
   return retries;
}

static bool
sge_gdi_map_pack_errors(int pack_ret, lList **answer_list) {
   DENTER(GDI_LAYER);

   switch (pack_ret) {
   case PACK_SUCCESS:
      break;
   case PACK_ENOMEM:
      DTRACE;
      answer_list_add_sprintf(answer_list, STATUS_ERROR2, ANSWER_QUALITY_ERROR,
                              MSG_GDI_MEMORY_NOTENOUGHMEMORYFORPACKINGGDIREQUEST);
      break;
   case PACK_FORMAT:
      DTRACE;
      answer_list_add_sprintf(answer_list, STATUS_ERROR3, ANSWER_QUALITY_ERROR,
                              MSG_GDI_REQUESTFORMATERROR);
      break;
   default:
      DTRACE;
      answer_list_add_sprintf(answer_list, STATUS_ERROR1, ANSWER_QUALITY_ERROR,
                              MSG_GDI_UNEXPECTEDERRORWHILEPACKINGGDIREQUEST);
      break;
   }
   DRETURN(pack_ret == PACK_SUCCESS);
}


ocs::gdi::Packet::Packet()
       : mutex(PTHREAD_MUTEX_INITIALIZER), cond(PTHREAD_COND_INITIALIZER), is_handled(false), is_intern_request(false),
         request_type(PACKET_GDI_REQUEST), commproc_id(0),
         response_id(0), gdi_session(0), version(0), uid(0), gid(0), amount(0), grp_array(nullptr),
         ds_type(0) {
   DENTER(TOP_LAYER);
   version = ocs::Version::get_version();
   memset(&pb, 0, sizeof(sge_pack_buffer));
   ocs::uti::condition_initialize(&cond);
   DRETURN_VOID;
}

ocs::gdi::Packet::~Packet() {
   DENTER(TOP_LAYER);

   for (auto *task : tasks) {
      delete task;
   }
   pthread_mutex_destroy(&mutex);
   pthread_cond_destroy(&cond);
   // in case of internal requests we borrow the supplementary groups from the component module
   // do not free them!
   if (!is_intern_request) {
      sge_free(&grp_array);
   }
   DRETURN_VOID;
}

int
ocs::gdi::Packet::append_task(gdi::Task *task) {
   DENTER(TOP_LAYER);
   tasks.push_back(task);
   DRETURN(tasks.size() - 1);
}

void
ocs::gdi::Packet::create_multi_answer(lList **malpp) {
   DENTER(TOP_LAYER);

   /*
    * make multi answer list and move all data contained in packet
    * into that structure
    */
   for (size_t id = 0; id < tasks.size(); ++id) {
      gdi::Task *task = tasks[id];
      lListElem *map = lAddElemUlong(malpp, MA_id, id, MA_Type);

      if (task->command == gdi::Command::SGE_GDI_GET ||
          task->command == gdi::Command::SGE_GDI_PERMCHECK ||
          (task->command == gdi::Command::SGE_GDI_ADD && task->sub_command == gdi::SubCommand::SGE_GDI_RETURN_NEW_VERSION)) {
         lSetList(map, MA_objects, task->data_list);
         task->data_list = nullptr;
      }

      lSetList(map, MA_answers, task->answer_list);
      task->answer_list = nullptr;
   }
   DRETURN_VOID;
}


/****** gdi/request_internal/sge_gdi_packet_wait_till_handled() *************
*  NAME
*     ocs::gdi::Client::sge_gdi_packet_wait_till_handled() -- wait til packet is handled
*
*  SYNOPSIS
*     void
*     ocs::gdi::Client::sge_gdi_packet_wait_till_handled(ocs::gdi::Packet *packet)
*
*  FUNCTION
*     This function blocks the calling thread till another one executes
*     ocs::gdi::Client::sge_gdi_packet_broadcast_that_handled(). Mutiple threads can use
*     this call to get response if the packet is accessed by someone
*     else anymore.
*
*     This function is used to synchronize packet producers (listerner,
*     scheduler, jvm thread ...) with packet consumers (worker threads)
*     which all use a packet queue to synchronize the access to
*     packet elements.
*
*     Packet producers store packets in the packet queue and then
*     they call this function to wait that they can access the packet
*     structure again.
*
*  INPUTS
*     ocs::gdi::Packet *packet - packet element
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: ocs::gdi::Client::sge_gdi_packet_wait_till_handled() is MT safe
*
*  SEE ALSO
*     gdi/request_internal/Master_Packet_Queue
*     gdi/request_internal/sge_gdi_packet_queue_wait_for_new_packet()
*     gdi/request_internal/sge_gdi_packet_queue_store_notify()
*     gdi/request_internal/sge_gdi_packet_broadcast_that_handled()
*     gdi/request_internal/sge_gdi_packet_is_handled()
*******************************************************************************/
void
ocs::gdi::Packet::wait_till_handled() {
   DENTER(TOP_LAYER);

   sge_mutex_lock(GDI_PACKET_MUTEX, __func__, __LINE__, &mutex);

   while (!is_handled) {
      DPRINTF("waiting for packet to be handling by worker\n");
      ocs::uti::condition_timedwait(&cond, &mutex, CLIENT_WAIT_TIME_S);
   }

   sge_mutex_unlock(GDI_PACKET_MUTEX, __func__, __LINE__, &mutex);

   DPRINTF("got signal that packet is handled\n");

   DRETURN_VOID;
}

/****** gdi/request_internal/sge_gdi_packet_is_handled() ********************
*  NAME
*     ocs::gdi::Client::sge_gdi_packet_is_handled() -- returns if packet was handled by worker
*
*  SYNOPSIS
*     void
*     ocs::gdi::Client::sge_gdi_packet_is_handled(ocs::gdi::Packet *packet)
*
*  FUNCTION
*     Returns if the given packet was already handled by a worker thread.
*     "true" means that the packet is completely done so that a call
*     to ocs::gdi::Client::sge_gdi_packet_wait_till_handled() will return immediately. If
*     "false" is returned the packet is not finished so a call to
*     ocs::gdi::Client::sge_gdi_packet_wait_till_handled() might block when it is called
*     afterwards.
*
*  INPUTS
*     ocs::gdi::Packet *packet - packet element
*
*  RESULT
*     bool - true    packet was already handled by a worker
*            false   packet is not done.
*
*  NOTES
*     MT-NOTE: ocs::gdi::Client::sge_gdi_packet_is_handled() is MT safe
*
*  SEE ALSO
*     gdi/request_internal/Master_Packet_Queue
*     gdi/request_internal/sge_gdi_packet_queue_wait_for_new_packet()
*     gdi/request_internal/sge_gdi_packet_queue_store_notify()
*     gdi/request_internal/sge_gdi_packet_broadcast_that_handled()
*******************************************************************************/
bool
ocs::gdi::Packet::get_is_handled() {
   bool ret = true;

   DENTER(TOP_LAYER);
   sge_mutex_lock(GDI_PACKET_MUTEX, __func__, __LINE__, &mutex);
   ret = is_handled;
   sge_mutex_unlock(GDI_PACKET_MUTEX, __func__, __LINE__, &mutex);
   DRETURN(ret);
}

void
ocs::gdi::Packet::broadcast_that_handled()
{
   DENTER(TOP_LAYER);

   sge_mutex_lock(GDI_PACKET_MUTEX, __func__, __LINE__, &mutex);
   is_handled = true;
   DPRINTF("broadcast that packet is handled\n");
   pthread_cond_broadcast(&cond);
   sge_mutex_unlock(GDI_PACKET_MUTEX, __func__, __LINE__, &mutex);

   DRETURN_VOID;
}

bool
ocs::gdi::Packet::execute_external(lList **answer_list)
{
   bool ret = true;
   sge_pack_buffer pb;
   bool pb_initialized = false;
   sge_pack_buffer rpb;
   int commlib_error;
   u_long32 message_id;

   DENTER(TOP_LAYER);

#ifdef KERBEROS
   /* request that the Kerberos library forward the TGT */
   if (ret && packet->first_task->target == SGE_JB_LIST &&
       packet->first_task->command == SGE_GDI_ADD ) {
      krb_set_client_flags(krb_get_client_flags() | KRB_FORWARD_TGT);
      krb_set_tgt_id(packet->id);
   }
#endif

    /*
     * Now we will execute the JSV script if we got a job submission request.
     * It is necessary to dechain the job which is verified because the
     * job verification process might destroy the job and create a completely
     * new one with adjusted job attributes.
     */
    gdi::Task *task = tasks[0];

    if (task->target == ocs::gdi::Target::TargetValue::SGE_JB_LIST &&
        (task->command == gdi::Command::SGE_GDI_ADD || task->command == gdi::Command::SGE_GDI_COPY)) {
       lListElem *job, *next_job;

       next_job = lLastRW(task->data_list);
       while (ret && ((job = next_job) != nullptr)) {
          next_job = lNextRW(job);

          lDechainElem(task->data_list, job);
          ret &= jsv_do_verify(JSV_CONTEXT_CLIENT, &job, answer_list, false);
          lInsertElem(task->data_list, nullptr, job);
       }
    }

   /*
    * pack packet into packbuffer
    */
   if (ret) {
      size_t size = get_pb_size();

      if (size > 0) {
         int pack_ret;

         pack_ret = init_packbuffer(&pb, size);
         if (pack_ret != PACK_SUCCESS) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, "unable to prepare packbuffer for sending request");
            answer_list_add(answer_list, SGE_EVENT, STATUS_NOQMASTER, ANSWER_QUALITY_ERROR);
            ret = false;
         } else {
            pb_initialized = true;
         }
      }
   }
   if (ret) {
      ret = pack(answer_list, &pb);
   }

   /*
    * send packbuffer to master. keep care that user does not see
    * commlib related error messages if master is not up and running
    */
   if (ret) {
      const char *tmp_commproc = prognames[QMASTER];
      const char *tmp_host = gdi::ClientBase::gdi_get_act_master_host(false);
      int tmp_id = 1;
      int tmp_response_id = 0;
      commlib_error = ClientServerBase::sge_gdi_send_any_request(0, &message_id, tmp_host, tmp_commproc, tmp_id, &pb,
                                                             ClientServerBase::TAG_GDI_REQUEST, tmp_response_id, nullptr);
      if (commlib_error != CL_RETVAL_OK) {
         commlib_error = ClientBase::gdi_is_alive(answer_list);
         if (commlib_error != CL_RETVAL_OK) {
            u_long32 sge_qmaster_port = bootstrap_get_sge_qmaster_port();
            const char *mastername = ClientBase::gdi_get_act_master_host(true);

            if (commlib_error == CL_RETVAL_CONNECT_ERROR ||
                commlib_error == CL_RETVAL_CONNECTION_NOT_FOUND ) {
               /* For the default case, just print a simple message */
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_UNABLE_TO_CONNECT_SUS, prognames[QMASTER],
                        sge_qmaster_port, mastername?mastername:"<nullptr>");
            } else {
               /* For unusual errors, give more detail */
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_CANT_SEND_MSG_TO_PORT_ON_HOST_SUSS, prognames[QMASTER],
                        sge_qmaster_port, mastername?mastername:"<nullptr>", cl_get_error_text(commlib_error));
            }
         } else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_GDI_SENDINGGDIREQUESTFAILED);
         }
         answer_list_add(answer_list, SGE_EVENT, STATUS_NOQMASTER, ANSWER_QUALITY_ERROR);
         ret = false;
      }

   }

   /* after this point we do no longer need pb - free its resources */
   if (pb_initialized) {
      clear_packbuffer(&pb);
   }

   /*
    * wait for response from master; also here keep care that commlib
    * related error messages are hidden if master is not up and running anymore
    */
   if (ret) {
      const char *commproc = prognames[QMASTER];
      const char *host = ClientBase::gdi_get_act_master_host(false);
      char rcv_host[CL_MAXHOSTNAMELEN+1];
      char rcv_commproc[CL_MAXHOSTNAMELEN+1];
      ClientServerBase::ClientServerBaseTag tag = ClientServerBase::TAG_GDI_REQUEST;
      u_short id = 1;
      int gdi_error = CL_RETVAL_OK;
      int runs = 0;
      int retries;

      strcpy(rcv_host, host);
      strcpy(rcv_commproc, commproc);

      /*running this loop as long as configured in gdi_retries, doing a break after getting a gdi_request*/
      do {

         // If GDI parameter (like gdi_timeout) changed then we will update the handle within this loop
         cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
         if (handle != nullptr) {
            char *gdi_timeout_str = nullptr;
            cl_com_get_parameter_list_value("gdi_timeout", &gdi_timeout_str);
            if (gdi_timeout_str != nullptr) {
               int gdi_timeout = atoi(gdi_timeout_str);
               sge_free(&gdi_timeout_str);
               cl_com_set_synchron_receive_timeout(handle, gdi_timeout);
            }
         }

         gdi_error = ClientServerBase::sge_gdi_get_any_request(rcv_host, rcv_commproc, &id, &rpb, &tag, true, message_id, nullptr);

         bool do_ping = get_cl_ping_value();
         retries = get_gdi_retries_value();

         if (gdi_error == CL_RETVAL_OK) {
            /*no error happened, leaving while*/
            ret = true;
            break;
         } else {
            ret = false;
            /*this error appears, if qmaster or any qmaster thread is not responding, or overloaded*/
            if (gdi_error == CL_RETVAL_SYNC_RECEIVE_TIMEOUT) {
               cl_com_SIRM_t* cl_endpoint_status = nullptr;
               DPRINTF("TEST_2372_OUTPUT: CL_RETVAL_SYNC_RECEIVE_TIMEOUT: RUNS=" sge_u32 "\n", runs);

               cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
               if (handle != nullptr) {
                  DPRINTF("TEST_2372_OUTPUT: GDI_TIMEOUT=%d\n", handle->synchron_receive_timeout);
               }
               if (do_ping) {
                  DPRINTF("TEST_2372_OUTPUT: CL_PING=TRUE\n");
                  cl_commlib_get_endpoint_status(handle, rcv_host, rcv_commproc, id, &cl_endpoint_status);
                  if (cl_endpoint_status != nullptr) {
                     if (cl_endpoint_status->application_status != 0) {
                        DPRINTF("TEST_2372_OUTPUT: QPING: error\n");
                     } else {
                        DPRINTF("TEST_2372_OUTPUT: QPING: ok\n");
                     }
                     cl_com_free_sirm_message(&cl_endpoint_status);
                  } else {
                     DPRINTF("TEST_2372_OUTPUT: QPING: failed\n");
                     break;
                  }
               } else {
                  DPRINTF("TEST_2372_OUTPUT: CL_PING=FALSE\n");
               }
            } else {
               break;
            }
         }
         /* only increment runs if retries != -1 (-1 means retry forever) */
      } while (retries == -1 || runs++ < retries);

      if (!ret) {
         commlib_error = ClientBase::gdi_is_alive(answer_list);
         if (commlib_error != CL_RETVAL_OK) {
            u_long32 sge_qmaster_port = bootstrap_get_sge_qmaster_port();
            const char *mastername = ClientBase::gdi_get_act_master_host(true);

            if (commlib_error == CL_RETVAL_CONNECT_ERROR ||
                commlib_error == CL_RETVAL_CONNECTION_NOT_FOUND ) {
               /* For the default case, just print a simple message */
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_UNABLE_TO_CONNECT_SUS, prognames[QMASTER],
                        sge_qmaster_port, mastername?mastername:"<nullptr>");
            } else {
               /* For unusual errors, give more detail */
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_CANT_SEND_MSG_TO_PORT_ON_HOST_SUSS, prognames[QMASTER],
                        sge_qmaster_port, mastername?mastername:"<nullptr>", cl_get_error_text(commlib_error));
            }
         } else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_RECEIVEGDIREQUESTFAILED_US, message_id, cl_get_error_text(gdi_error));
         }
         answer_list_add(answer_list, SGE_EVENT, STATUS_NOQMASTER, ANSWER_QUALITY_ERROR);
      }
   }

   /*
    * unpack result. the returned packet contains data and/or answer lists
    */
   Packet *ret_packet = nullptr;
   if (ret) {
      ret_packet = new Packet();
      ret = ret_packet->unpack(answer_list, &rpb);
      if (!ret) {
         DTRACE;
         delete ret_packet;
      }
      clear_packbuffer(&rpb);
   }

   /*
    * consistency check of received data:
    *    - is the packet id the same
    *    - does it contain the same amount of tasks
    *    - is the task sequence and the task id of each received task the same
    */
   if (ret) {
      DTRACE;
      bool gdi_mismatch = false;

      if (!gdi_mismatch && tasks.size() != ret_packet->tasks.size()) {
         gdi_mismatch = true;
      }

      if (!gdi_mismatch) {
         for (size_t i = 0; i < tasks.size(); i++) {
            gdi::Task *send = tasks[i];
            gdi::Task *recv = ret_packet->tasks[i];

            lFreeList(&send->data_list);
            send->data_list = recv->data_list;
            send->answer_list = recv->answer_list;
            recv->data_list = nullptr;
            recv->answer_list = nullptr;
         }
      }
      if (gdi_mismatch) {
         /* For unusual errors, give more detail */
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_GDI_MISMATCH_SEND_RECEIVE);
         answer_list_add(answer_list, SGE_EVENT, STATUS_NOQMASTER, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }

#ifdef KERBEROS
   /* clear the forward TGT request */
   if (ret && state->first->target == SGE_JB_LIST &&
       packet->first_task->command == SGE_GDI_ADD) {
      krb_set_client_flags(krb_get_client_flags() & ~KRB_FORWARD_TGT);
      krb_set_tgt_id(0);
   }
#endif

   delete ret_packet;
   DRETURN(ret);
}

bool
ocs::gdi::Packet::execute_internal(lList **answer_list) {
   DENTER(TOP_LAYER);

   // fill in packet data for internal request - we don't get it via a pack buffer
   strncpy(commproc, prognames[QMASTER], sizeof(commproc)-1);
   strncpy(host, ClientBase::gdi_get_act_master_host(false), sizeof(host)-1);
   // @todo make a function for filling in all the user data in a single call
   uid = component_get_uid();
   gid = component_get_gid();
   sge_strlcpy(user, component_get_username(), sizeof(user));
   sge_strlcpy(group, component_get_groupname(), sizeof(group));
   component_get_supplementray_groups(&amount, &grp_array);
   is_intern_request = true;

   bool ret = true;

   /*
    * append the packet to the packet list of the worker threads
    */
   sge_tq_store_notify(GlobalRequestQueue, SGE_TQ_GDI_PACKET, this);
   DRETURN(ret);
}

void
ocs::gdi::Packet::wait_for_result_external(lList **malpp) {
   DENTER(TOP_LAYER);

   /*
    * The packet itself has already be executed in ocs::gdi::Client::sge_gdi_packet_execute_external()
    * so it is only necessary to create the muti answer and do cleanup
    */
   create_multi_answer(malpp);

   DRETURN_VOID;
}

void
ocs::gdi::Packet::wait_for_result_internal(lList **malpp) {
   DENTER(TOP_LAYER);
   wait_till_handled();
   create_multi_answer(malpp);
   DRETURN_VOID;
}

u_long32
ocs::gdi::Packet::get_pb_size() {
   DENTER(TOP_LAYER);
   u_long32 ret = 0;
   bool local_ret;
   lList *local_answer_list = nullptr;
   sge_pack_buffer tmppb;

   init_packbuffer(&tmppb, 0, true, false);
   local_ret = pack(&local_answer_list, &tmppb);
   if (local_ret) {
      ret = pb_used(&tmppb);
   }
   clear_packbuffer(&tmppb);
   DRETURN(ret);
}

bool
ocs::gdi::Packet::unpack(lList **answer_list, sge_pack_buffer *pb) {
   DENTER(TOP_LAYER);
   bool ret = true;
   bool has_next;
   int pack_ret;


   unpack_header(answer_list, pb);

   do {
      auto task = new gdi::Task(gdi::Target::NO_TARGET, gdi::Command::SGE_GDI_NONE, gdi::SubCommand::SGE_GDI_SUB_NONE,
                              nullptr, nullptr, nullptr, nullptr, false);
      u_long32 target_ulong32 = 0;
      u_long32 command = 0;
      u_long32 sub_command = 0;
      lList *data_list = nullptr;
      lList *a_list = nullptr;
      lCondition *condition = nullptr;
      lEnumeration *enumeration = nullptr;
      u_long32 has_next_int = 0;

      if ((pack_ret = unpackint(pb, &command))) {
         goto error_with_mapping;
      }
      task->command = static_cast<gdi::Command::Cmd>(command);

      if ((pack_ret = unpackint(pb, &sub_command))) {
         goto error_with_mapping;
      }
      task->sub_command = static_cast<gdi::SubCommand::SubCmd>(sub_command);

      if ((pack_ret = unpackint(pb, &target_ulong32))) {
         goto error_with_mapping;
      }
      task->target = static_cast<gdi::Target::TargetValue>(target_ulong32);

      if ((pack_ret = cull_unpack_list(pb, &(data_list)))) {
         goto error_with_mapping;
      }
      task->data_list = data_list;

      if ((pack_ret = cull_unpack_list(pb, &(a_list)))) {
         goto error_with_mapping;
      }
      task->answer_list = a_list;

      if ((pack_ret = cull_unpack_cond(pb, &(condition)))) {
         goto error_with_mapping;
      }
      task->condition = condition;

      if ((pack_ret = cull_unpack_enum(pb, &(enumeration)))) {
         goto error_with_mapping;
      }
      task->enumeration = enumeration;

      if ((pack_ret = unpackint(pb, &has_next_int))) {
         goto error_with_mapping;
      }
      has_next = (has_next_int > 0) ? true : false;

      append_task(task);
   } while (has_next);

#if 0
   debug_print();
#endif

   DRETURN(ret);
error_with_mapping:
   ret = sge_gdi_map_pack_errors(pack_ret, answer_list);
   DRETURN(ret);
}

bool
ocs::gdi::Packet::unpack_header(lList **answer_list, sge_pack_buffer *pb) {
   DENTER(TOP_LAYER);
   bool ret;
   int pack_ret;
   u_long32 tmp_version = 0;

   if ((pack_ret = unpackint(pb, &tmp_version))) {
      goto error_with_mapping;
   }
   version = tmp_version;
   /* JG: TODO (322): At this point we should check the version!
    **                 The existent check function ocs::gdi::Client::sge_gdi_packet_verify_version
    **                 cannot be called as necessary data structures are
    **                 available here (e.g. answer list).
    **                 Better do these changes at a more general place
    **                 together with (hopefully coming) further communication
    **                 redesign.
    */
error_with_mapping:
   ret = sge_gdi_map_pack_errors(pack_ret, answer_list);
   DRETURN(ret);
}

bool
ocs::gdi::Packet::pack(lList **answer_list, sge_pack_buffer *pb) {
   DENTER(TOP_LAYER);
   bool ret = true;

#if 0
   debug_print();
#endif

   ret = pack_header(answer_list, pb);
   if (!ret) {
      DRETURN(ret);
   }

   for (size_t i = 0; i < tasks.size(); ++i) {
      bool has_next = (i < tasks.size() - 1);

      if (const bool ret = pack_task(tasks[i], answer_list, pb, has_next); !ret) {
         return ret;
      }
   }
   DRETURN(true);
}

bool
ocs::gdi::Packet::pack_header(lList **answer_list, sge_pack_buffer *pb) {
   DENTER(TOP_LAYER);
   bool ret = true;
   int pack_ret;

   pack_ret = packint(pb, version);
   if (pack_ret != PACK_SUCCESS) {
      goto error_with_mapping;
   }

   DRETURN(true);
error_with_mapping:
   ret = sge_gdi_map_pack_errors(pack_ret, answer_list);
   DRETURN(ret);
}

bool
ocs::gdi::Packet::pack_task(ocs::gdi::Task *task, lList **answer_list, sge_pack_buffer *pb, bool has_next) {
   DENTER(TOP_LAYER);
   bool ret = true;
   int pack_ret;

   if (task != nullptr && !is_intern_request) {
      pack_ret = packint(pb, task->command);
      if (pack_ret != PACK_SUCCESS) {
         goto error_with_mapping;
      }

      pack_ret = packint(pb, task->sub_command);
      if (pack_ret != PACK_SUCCESS) {
         goto error_with_mapping;
      }

      pack_ret = packint(pb, task->target);
      if (pack_ret != PACK_SUCCESS) {
         goto error_with_mapping;
      }

      /*
       * if the lSelect call was postponed then it will be done here.
       * here we are able to pack the result list directly into the packbuffer.
       * additionally it is necessary to add an answer to the answer list.
       * (which will be packed below).
       */
      if (task->do_select_pack_simultaneous) {
         lSelectHashPack("", task->data_list, task->condition, task->enumeration, false, pb);
         lFreeWhat(&(task->enumeration));
         lFreeWhere(&(task->condition));
         task->data_list = nullptr;

         /* DIRTY HACK: The "ok" message should be removed from the answer list
          * 05/21/2007 quality was ANSWER_QUALITY_INFO but this results in "ok"
          * messages on qconf side */
         answer_list_add(&(task->answer_list), MSG_GDI_OKNL, STATUS_OK, ANSWER_QUALITY_END);
      } else {
         /* ===> pack the list */
         pack_ret = cull_pack_list(pb, task->data_list);
         if (pack_ret != PACK_SUCCESS) {
            goto error_with_mapping;
         }
      }

      /* ===> pack the suffix */
      pack_ret = cull_pack_list(pb, task->answer_list);
      if (pack_ret != PACK_SUCCESS) {
         goto error_with_mapping;
      }
      pack_ret = cull_pack_cond(pb, task->condition);
      if (pack_ret != PACK_SUCCESS) {
         goto error_with_mapping;
      }
      pack_ret = cull_pack_enum(pb, task->enumeration);
      if (pack_ret != PACK_SUCCESS) {
         goto error_with_mapping;
      }

      pack_ret = packint(pb, has_next ? 1 : 0);
      if (pack_ret != PACK_SUCCESS) {
         goto error_with_mapping;
      }
   }

   DRETURN(ret);
 error_with_mapping:
   ret = sge_gdi_map_pack_errors(pack_ret, answer_list);
   if (task->do_select_pack_simultaneous) {
      // data_list references a master list
      // avoid it being freed when the packet/task gets freed
      task->data_list = nullptr;
   }
   DRETURN(ret);
}

void ocs::gdi::Packet::debug_print() {
   DENTER(TOP_LAYER);

   DPRINTF("packet->host = " SFQ "\n", host);
   DPRINTF("packet->commproc = " SFQ "\n", commproc);
   DPRINTF("packet->version = " sge_u32 "\n", version);
   DPRINTF("packet->tasks = %d\n", tasks.size());

   for (auto *task : tasks) {
      task->debug_print();
   }
   DRETURN_VOID;
}

