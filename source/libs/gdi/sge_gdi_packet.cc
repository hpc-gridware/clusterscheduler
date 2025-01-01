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
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>

#ifdef KERBEROS
#include "krb_lib.h"
#endif

#include "basis_types.h"

#include "comm/commlib.h"

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_thread_ctrl.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/ocs_Version.h"

#include "gdi/sge_gdi_packet.h"
#include "gdi/sge_security.h"
#include "gdi/sge_gdi.h"

#include "msg_common.h"

/****** gdi/request_internal/--Packets_and_Taks() *****************************
*  NAME
*     Packets_and_Tasks -- Functions and structures behind GDI
*
*  FUNCTION
*     Packets and tasks are structures which are used in the methods 
*     implementing GDI. The real C structure names are 
*
*        "sge_gdi_packet_cass_t" and 
*        "ocs::GdiTask".
*
*     An instance of the packet structure represents a GDI request.
*     Each packet has assigned at least one task. But it may have
*     multiple tasks. A Packet with one task represents a
*     singe GDI request and that one with multiple task represents
*     a multi GDI request.
*
*     GDI requests are created by funtion calls sge_gdi() and
*     sge_gdi_multi(). Internally the packet and task data 
*     structures are used to represent that request information.
*
*     Within the client side code (sge_gdi(), sge_gdi_multi(),
*     sge_gdi_wait(), sge_extract_answer() ... ) there are multiple
*     functions used to create and verify packets and tasks:
*
*        sge_gdi_packet_append_task()
*        sge_gdi_packet_create()
*        sge_gdi_packet_create_base()
*        sge_gdi_packet_create_multi_answer()
*        sge_gdi_packet_free()
*        sge_gdi_packet_get_last_task_id()
*        sge_gdi_packet_initialize_auth_info()
*        sge_gdi_task_create()
*        sge_gdi_task_free()
*
*     Following functions are used to handle data transfer
*     from external GDI clients (qstat, qconf, ...) or internal
*     GDI clients (scheduler, JVM, ...)
*
*        sge_gdi_packet_execute_internal()
*        sge_gdi_packet_execute_external()
*        sge_gdi_packet_wait_for_result_internal()
*        sge_gdi_packet_wait_for_result_external()
*
*     Data transfer of packets transfered  between external GDI 
*     clients is prepared with following functions process:
*
*        sge_gdi_packet_get_pb_size()
*        sge_gdi_packet_initialize_auth_info()
*        sge_gdi_packet_pack()
*        sge_gdi_packet_pack_task()
*        sge_gdi_packet_parse_auth_info()
*        sge_gdi_packet_unpack()
*
*    Within the master process special syncronisation
*    for the thread accessing packet and task datastructures
*    is required because the data structures are not thread
*    safe per default. Only one thread is allowed to access
*    a packet or task structure at the same time. To synchronize
*    their activities between "listerner" and "worker"
*    threads a queue of packet elements is used. This
*    "Master_Packet_Queue" is guarded by the funtion calls
*
*        sge_gdi_packet_queue_wait_for_new_packet()
*        sge_gdi_packet_queue_store_notify()
*        sge_gdi_packet_queue_wakeup_all_waiting()
*
*     Additional synchronisation functions are
*
*        sge_gdi_packet_wait_till_handled()
*        sge_gdi_packet_broadcast_that_handled()
*
*  SEE ALSO
*     gdi/request_internal/Master_Packet_Queue
*     gdi/request_internal/sge_gdi_task_create()
*     gdi/request_internal/sge_gdi_task_free()
*     gdi/request_internal/sge_gdi_task_get_operation_name()
*     gdi/request_internal/sge_gdi_packet_append_task()
*     gdi/request_internal/sge_gdi_packet_broadcast_that_handled()
*     gdi/request_internal/sge_gdi_packet_create()
*     gdi/request_internal/sge_gdi_packet_create_base()
*     gdi/request_internal/sge_gdi_packet_create_multi_answer()
*     gdi/request_internal/sge_gdi_packet_get_last_task_id()
*     gdi/request_internal/sge_gdi_packet_execute_internal()
*     gdi/request_internal/sge_gdi_packet_execute_external()
*     gdi/request_internal/sge_gdi_packet_free()
*     gdi/request_internal/sge_gdi_packet_get_pb_size()
*     gdi/request_internal/sge_gdi_packet_initialize_auth_info()
*     gdi/request_internal/sge_gdi_packet_pack()
*     gdi/request_internal/sge_gdi_packet_pack_task()
*     gdi/request_internal/sge_gdi_packet_parse_auth_info()
*     gdi/request_internal/sge_gdi_packet_queue_store_notify()
*     gdi/request_internal/sge_gdi_packet_queue_wait_for_new_packet()
*     gdi/request_internal/sge_gdi_packet_queue_wakeup_all_waiting()
*     gdi/request_internal/sge_gdi_packet_unpack()
*     gdi/request_internal/sge_gdi_packet_initialize_auth_info()
*     gdi/request_internal/sge_gdi_packet_wait_for_result_internal()
*     gdi/request_internal/sge_gdi_packet_wait_for_result_external()
*     gdi/request_internal/sge_gdi_packet_wait_till_handled()
*******************************************************************************/

#if 1
#define SGE_GDI_PACKET_DEBUG
#endif

#if defined(SGE_GDI_PACKET_DEBUG)

void
sge_gdi_packet_debug_print(sge_gdi_packet_class_t *packet);

#endif

void
sge_gdi_task_debug_print(ocs::GdiTask *task) {
   DENTER(TOP_LAYER);
   if (task != nullptr) {
      DPRINTF("task->id = " sge_U32CFormat "\n", sge_u32c(task->id));
      DPRINTF("task->command = " sge_U32CFormat "\n", sge_u32c(task->command));
      DPRINTF("task->target = " sge_U32CFormat "\n", sge_u32c(task->target));
      DPRINTF("task->data_list = %p\n", task->data_list);
      DPRINTF("task->answer_list = %p\n", task->answer_list);
      DPRINTF("task->condition = %p\n", task->condition);
      DPRINTF("task->enumeration = %p\n", task->enumeration);
   } else {
      DPRINTF("task is nullptr\n");
   }
   DRETURN_VOID;
}

#if defined(SGE_GDI_PACKET_DEBUG)

void
sge_gdi_packet_debug_print(sge_gdi_packet_class_t *packet) {
   DENTER(TOP_LAYER);

   if (packet != nullptr) {
      DPRINTF("packet->id = " sge_U32CFormat "\n", sge_u32c(packet->id));
      DPRINTF("packet->host = " SFQ "\n", packet->host);
      DPRINTF("packet->commproc = " SFQ "\n", packet->commproc);
      DPRINTF("packet->auth_info = " SFQ "\n", packet->auth_info ? packet->auth_info : "<null>");
      DPRINTF("packet->version = " sge_U32CFormat "\n", sge_u32c(packet->version));
      DPRINTF("packet->tasks = %d\n", packet->tasks.size());

      for (auto *task : packet->tasks) {
         sge_gdi_task_debug_print(task);
      }
   } else {
      DPRINTF("packet is nullptr\n");;
   }
   DRETURN_VOID;
}

#endif

/****** gdi/request_internal/sge_gdi_packet_create_base() ********************
*  NAME
*     sge_gdi_packet_create_base() -- ???
*
*  SYNOPSIS
*     sge_gdi_packet_class_t *
*     sge_gdi_packet_create_base(lList **answer_list)
*
*  FUNCTION
*     Creates a new GDI packet and initializes all base structure memebers
*     where necessary information is available.
*
*     "uid", "gid", "user" and "group" memebers part of sge_gdi_packet_class_t
*     will not be initialized with this function. Instead
*     sge_gdi_packet_create() can be used or the function
*     sge_gdi_packet_initialize_auth_info() can be called afterwards.
*
*  INPUTS
*     lList **answer_list - answer list in case of error
*
*  RESULT
*     sge_gdi_packet_class_t * - new GDI packet
*
*  NOTES
*     MT-NOTE: sge_gdi_packet_create_base() is MT safe
*
*  SEE ALSO
*     gdi/request_internal/sge_gdi_packet_create()
*     gdi/request_internal/sge_gdi_packet_initialize_auth_info()
******************************************************************************/
sge_gdi_packet_class_t *
sge_gdi_packet_create_base(lList **answer_list) {
   DENTER(TOP_LAYER);
   auto ret = new sge_gdi_packet_class_t();

   int local_ret1 = pthread_mutex_init(&(ret->mutex), nullptr);
   int local_ret2 = pthread_cond_init(&(ret->cond), nullptr);
   if (local_ret1 != 0 || local_ret2 != 0) {
      answer_list_add_sprintf(answer_list, STATUS_EMALLOC, ANSWER_QUALITY_ERROR, MSG_MEMORY_MALLOCFAILED);
      sge_free(&ret);
      DRETURN(nullptr);
   }

   ret->request_type = PACKET_GDI_REQUEST;
   ret->version = ocs::Version::get_version();
   memset(&(ret->pb), 0, sizeof(sge_pack_buffer));

   DRETURN(ret);
}

/****** gdi/request_internal/sge_gdi_packet_create() **************************
*  NAME
*     sge_gdi_packet_create() -- create a new GDI packet and initialize it
*
*  SYNOPSIS
*     sge_gdi_packet_class_t *
*     sge_gdi_packet_create(sge_gdi_ctx_class_t *ctx, lList **answer_list)
*
*  FUNCTION
*     Creates a new GDI packet element and initializes all members of the
*     structure.
*
*  INPUTS
*     sge_gdi_ctx_class_t *ctx - GDI context
*     lList **answer_list      - answer list
*
*  RESULT
*     sge_gdi_packet_class_t * - new packet element or nullptr in case of errors
*
*  NOTES
*     MT-NOTE: sge_gdi_packet_create() is MT safe
*
*  SEE ALSO
*     gdi/request_internal/sge_gdi_packet_create_base()
*     gdi/request_internal/sge_gdi_packet_initialize_auth_info()
******************************************************************************/
sge_gdi_packet_class_t *
sge_gdi_packet_create(lList **answer_list) {
   DENTER(TOP_LAYER);
   sge_gdi_packet_class_t *ret = sge_gdi_packet_create_base(answer_list);
   if (ret != nullptr) {
      sge_gdi_packet_initialize_auth_info(ret);
   }
   DRETURN(ret);
}

/****** gdi/request_internal/sge_gdi_packet_append_task() *********************
*  NAME
*     sge_gdi_packet_append_task() -- append an additional GDI task
*
*  SYNOPSIS
*     bool
*     sge_gdi_packet_append_task(sge_gdi_packet_class_t *packet,
*                                lList **answer_list, u_long32 target,
*                                u_long32 command, lList **lp,
*                                lList **a_list, lCondition **condition,
*                                lEnumeration **enumeration,
*                                bool do_copy, bool do_verify)
*
*  FUNCTION
*     This function creates a new GDI task representing one
*     request of a multi GDI request. The task will be appended to
*     the list of tasks part of "packet". It will be initialized
*     with the values given by "target" and "command". Pointer
*     parameters like "lp", "a_list", "condition", and "enumeration"
*     will either be copied ("do_copy" is true) or they will just be
*     used as they are. In that case they will direct to nullptr after
*     the function returns. The memory allocated by the provided
*     pointer parameters will be released when the task is destroyed
*     they are part of. "do_verify" defines if the input parameters
*     are verified after the task element has been created.
*
*     In case of any error the function will return "false"
*     and the "answer_list" will be filled with a message.
*     Causes for errors are:
*        - malloc failure
*
*  INPUTS
*     sge_gdi_packet_class_t *packet - packet
*     lList **answer_list            - answer list used by this function
*     u_long32 target                - GDI target value
*     u_long32 command               - GDI command
*     lList **lp                     - list pointer
*     lList **a_list                 - answer list pointer
*     lCondition **condition         - CULL condition
*     lEnumeration **enumeration     - CULL enumeration
*     bool do_copy                   - do copy all elements passed
*                                      to this structure
*     bool do_verify                 - do verification of input data
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error (answer_list will be filled with detailed information)
*
*  NOTES
*     MT-NOTE: sge_gdi_packet_append_task() is MT safe
*
*  SEE ALSO
*     gdi/request_internal/sge_gdi_task_create()
******************************************************************************/
int
sge_gdi_packet_append_task(sge_gdi_packet_class_t *packet, lList **answer_list, ocs::GdiTarget::Target target, u_long32 command,
                           lList **lp, lList **a_list, lCondition **condition, lEnumeration **enumeration, bool do_copy) {
   DENTER(TOP_LAYER);
   auto task = new ocs::GdiTask(packet->tasks.size() + 1, answer_list, target, command, lp, a_list, condition, enumeration, do_copy);
   packet->tasks.push_back(task);
   DRETURN(task->id);
}

/****** gdi/request_internal/sge_gdi_task_get_operation_name() ****************
*  NAME
*     sge_gdi_task_get_operation_name() -- get command name 
*
*  SYNOPSIS
*     const char * 
*     sge_gdi_task_get_operation_name(ocs::GdiTask *task)
*
*  FUNCTION
*     This function returns a string of represending the command type
*     of a task part of a multi GDI request (e.g the function will return
*     "GET" when (task->command == SGE_GDI_GET))
*
*  INPUTS
*     ocs::GdiTask *task - gdi task
*
*  RESULT
*     const char * - string
*
*  NOTES
*     MT-NOTE: sge_gdi_task_get_operation_name() is MT safe 
*******************************************************************************/
const char *
sge_gdi_task_get_operation_name(ocs::GdiTask *task) {
   const char *ret;
   int operation = SGE_GDI_GET_OPERATION(task->command);

   switch (operation) {
      case SGE_GDI_GET:
         ret = "GET";
         break;
      case SGE_GDI_ADD:
         ret = "ADD";
         break;
      case SGE_GDI_DEL:
         ret = "DEL";
         break;
      case SGE_GDI_MOD:
         ret = "MOD";
         break;
      case SGE_GDI_COPY:
         ret = "COPY";
         break;
      case SGE_GDI_TRIGGER:
         ret = "TRIGGER";
         break;
      case SGE_GDI_PERMCHECK:
         ret = "PERMCHECK";
         break;
      case SGE_GDI_REPLACE:
         ret = "REPLACE";
         break;
      default:
         ret = "???";
         break;
   }
   return ret;
}

/****** gdi/request_internal/sge_gdi_packet_free() ****************************
*  NAME
*     sge_gdi_packet_free() -- free memory allocated by packet
*
*  SYNOPSIS
*     bool sge_gdi_packet_free(sge_gdi_packet_class_t **packet)
*
*  FUNCTION
*     Releases the memory allocated by all members of ther "packet"
*     structure including the list of tasks and sublists part of the
*     tasks.
*
*     The caller has to take care that the packet is not part of
*     a packet queue when this function is called!
*
*  INPUTS
*     sge_gdi_packet_class_t **packet - packet
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*
*  NOTES
*     MT-NOTE: sge_gdi_packet_free() is not MT safe
*
*  SEE ALSO
*     gdi/request_internal/sge_gdi_task_create()
*     gdi/request_internal/sge_gdi_packet_create()
******************************************************************************/
bool
sge_gdi_packet_free(sge_gdi_packet_class_t **packet) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (packet != nullptr && *packet != nullptr) {
      for (auto *task : (*packet)->tasks) {
         delete task;
      }
      int local_ret1 = pthread_mutex_destroy(&((*packet)->mutex));
      int local_ret2 = pthread_cond_destroy(&((*packet)->cond));
      if (local_ret1 != 0 || local_ret2 != 0) {
         ret = false;
      }
      sge_free(&(*packet)->auth_info);
      sge_free(&(*packet)->grp_array);
      delete *packet;
   }
   DRETURN(ret);
}
