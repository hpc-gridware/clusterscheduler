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
 *  Portions of this software are Copyright (c) 2011 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <pthread.h>

#include <pwd.h>

#include <unistd.h>

#include "comm/commlib.h"

#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_env.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_feature.h"
#include "sgeobj/cull/sge_multi_MA_L.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/cull/sge_permission_PERM_L.h"

#include "gdi/qm_name.h"
#include "gdi/sge_gdi.h"
#include "gdi/sge_gdi_data.h"
#include "sgeobj/sge_daemonize.h"
#include "gdi/sge_security.h"
#include "gdi/sge_gdi.h"
#include "gdi/sge_gdi_packet.h"
#include "gdi/sge_gdi_packet_internal.h"
#include "gdi/msg_gdilib.h"

#include "basis_types.h"
#include "msg_common.h"
#include "uti/sge.h"

#ifdef KERBEROS
#  include "krb_lib.h"
#endif

static void
dump_send_info(const char *comp_host, const char *comp_name, int comp_id,
               cl_xml_ack_type_t ack_type, unsigned long tag, unsigned long *mid);

static void
dump_receive_info(cl_com_message_t **message, cl_com_endpoint_t **sender);

static const char *
target2string(u_long32 target);

static int
gdi_send_message(int synchron, const char *tocomproc, int toid, const char *tohost, int tag, char **buffer,
                  int buflen, u_long32 *mid);


static const char *
target2string(u_long32 target) {
   const char *ret;

   switch (target) {
      case SGE_AH_LIST:
         ret = "SGE_AH_LIST";
         break;
      case SGE_SH_LIST:
         ret = "SGE_SH_LIST";
         break;
      case SGE_EH_LIST:
         ret = "SGE_EH_LIST";
         break;
      case SGE_CQ_LIST:
         ret = "SGE_CQ_LIST";
         break;
      case SGE_JB_LIST:
         ret = "SGE_JB_LIST";
         break;
      case SGE_EV_LIST:
         ret = "SGE_EV_LIST";
         break;
      case SGE_CE_LIST:
         ret = "SGE_CE_LIST";
         break;
      case SGE_ORDER_LIST:
         ret = "SGE_ORDER_LIST";
         break;
      case SGE_MASTER_EVENT:
         ret = "SGE_MASTER_EVENT";
         break;
      case SGE_CONF_LIST:
         ret = "SGE_CONF_LIST";
         break;
      case SGE_UM_LIST:
         ret = "SGE_UM_LIST";
         break;
      case SGE_UO_LIST:
         ret = "SGE_UO_LIST";
         break;
      case SGE_PE_LIST:
         ret = "SGE_PE_LIST";
         break;
      case SGE_SC_LIST:
         ret = "SGE_SC_LIST";
         break;
      case SGE_UU_LIST:
         ret = "SGE_UU_LIST";
         break;
      case SGE_US_LIST:
         ret = "SGE_US_LIST";
         break;
      case SGE_PR_LIST:
         ret = "SGE_PR_LIST";
         break;
      case SGE_STN_LIST:
         ret = "SGE_STN_LIST";
         break;
      case SGE_CK_LIST:
         ret = "SGE_CK_LIST";
         break;
      case SGE_CAL_LIST:
         ret = "SGE_CAL_LIST";
         break;
      case SGE_SME_LIST:
         ret = "SGE_SME_LIST";
         break;
      case SGE_ZOMBIE_LIST:
         ret = "SGE_ZOMBIE_LIST";
         break;
      case SGE_USER_MAPPING_LIST:
         ret = "SGE_USER_MAPPING_LIST";
         break;
      case SGE_HGRP_LIST:
         ret = "SGE_HGRP_LIST";
         break;
      case SGE_RQS_LIST:
         ret = "SGE_RQS_LIST";
         break;
      case SGE_AR_LIST:
         ret = "SGE_AR_LIST";
         break;
      default:
         ret = "unknown list";
   }
   return ret;
}

const char *
gdi_get_act_master_host(bool reread) {
   sge_error_class_t *eh = gdi_data_get_error_handle();
   static bool error_already_logged = false;

   DENTER(BASIS_LAYER);

   if (gdi_data_get_master_host() == nullptr || reread) {
      char err_str[SGE_PATH_MAX + 128];
      char master_name[CL_MAXHOSTNAMELEN];
      u_long64 now = sge_get_gmt64();

      /* fix system clock moved back situation */
      if (gdi_data_get_timestamp_qmaster_file() > now) {
         gdi_data_set_timestamp_qmaster_file(0);
      }

      if (gdi_data_get_master_host() == nullptr || now - gdi_data_get_timestamp_qmaster_file() >= sge_gmt32_to_gmt64(30)) {
         /* re-read act qmaster file (max. every 30 seconds) */
         DPRINTF("re-read actual qmaster file\n");
         gdi_data_set_timestamp_qmaster_file(now);

         if (get_qm_name(master_name, bootstrap_get_act_qmaster_file(), err_str, sizeof(err_str)) == -1) {
            if (eh != nullptr && !error_already_logged) {
               eh->error(eh, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_GDI_READMASTERNAMEFAILED_S, err_str);
               error_already_logged = true;
            }
            DRETURN(nullptr);
         }
         error_already_logged = false;
         DPRINTF("(re-)reading act_qmaster file. Got master host \"%s\"\n", master_name);
         /*
         ** TODO: thread locking needed here ?
         */
         gdi_set_master_host(master_name);
      }
   }
   DRETURN(gdi_data_get_master_host());
}

int
gdi_is_alive(lList **answer_list) {
   DENTER(TOP_LAYER);
   cl_com_SIRM_t *status = nullptr;
   int cl_ret = CL_RETVAL_OK;
   cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
   const char *comp_name = prognames[QMASTER];
   const char *comp_host = gdi_get_act_master_host(false);
   int comp_id = 1;
   u_long32 comp_port = bootstrap_get_sge_qmaster_port();

   if (handle == nullptr) {
#if 0
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "handle not found %s:0", component_get_component_name());
#endif
      DRETURN(CL_RETVAL_PARAMS);
   }

   /*
    * update endpoint information of qmaster in commlib
    * qmaster could have changed due to migration
    */
   cl_com_append_known_endpoint_from_name((char *) comp_host, (char *) comp_name, comp_id,
                                          (int)comp_port, CL_CM_AC_DISABLED, true);

   DPRINTF("to->comp_host, to->comp_name, to->comp_id: %s/%s/%d\n", comp_host ? comp_host : "", comp_name ? comp_name : "", comp_id);
   cl_ret = cl_commlib_get_endpoint_status(handle, (char *) comp_host, (char *) comp_name, comp_id, &status);
   if (cl_ret != CL_RETVAL_OK) {
#if 0
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "cl_commlib_get_endpoint_status failed: " SFQ, cl_get_error_text(cl_ret));
#endif
   } else {
      DEBUG(SFNMAX, MSG_GDI_QMASTER_STILL_RUNNING);
   }

   if (status != nullptr) {
      DEBUG(MSG_GDI_ENDPOINT_UPTIME_UU, sge_u32c(status->runtime), sge_u32c(status->application_status));
      cl_com_free_sirm_message(&status);
   }

   DRETURN(cl_ret);
}

/****** gdi/request/gdi_extract_answer() **********************************
*  NAME
*     gdi_extract_answer() -- exctact answers of a multi request.
*
*  SYNOPSIS
*     lList* gdi_extract_answer(u_long32 cmd, u_long32 target,
*                                   int id, lList* mal, lList** olpp) 
*
*  FUNCTION
*     This function extracts the answer for each invidual request on 
*     previous sge_gdi_multi() calls. 
*
*  INPUTS
*     lList** alpp    - 
*        List for answers if an error occurs in gdi_extract_answer
*        This list gets allocated by GDI. The caller is responsible for 
*        freeing. A response is a list element containing a field with a 
*        status value (AN_status). The value STATUS_OK is used in case of 
*        success. STATUS_OK and other values are defined in sge_answerL.h. 
*        The second field (AN_text) in a response list element is a string 
*        that describes the performed operation or a description of an error.
*
*     u_long32 cmd    - 
*        bitmask which decribes the operation 
*        (see sge_gdi_multi)
*
*     u_long32 target - 
*        unique id to identify masters list
*        (see sge_gdi_multi) 
*
*     int id          - 
*        unique id returned by a previous
*        sge_gdi_multi() call. 
*
*     lList* mal      - List of answer/response lists returned from
*        sge_gdi_multi(mode=SGE_GDI_SEND)
*
*     lList** olpp    - 
*        This parameter is used to get a list in case of SGE_GDI_GET command. 
         The caller is responsible for freeing by using lFreeList(). 
*
*  RESULT
*     true   in case of success
*     false  in case of error
*
*  NOTES
*     MT-NOTE: gdi_extract_answer() is MT safe
******************************************************************************/
bool
gdi_extract_answer(lList **alpp, u_long32 cmd, u_long32 target, int id, lList *mal, lList **olpp) {
   DENTER(GDI_LAYER);
   int operation = SGE_GDI_GET_OPERATION(cmd);
   int sub_command = SGE_GDI_GET_SUBCOMMAND(cmd);

   if (!mal || id < 0) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   lListElem *map = lGetElemUlongRW(mal, MA_id, id);
   if (!map) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_GDI_SGEGDIFAILED_S, target2string(target));
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   if ((operation == SGE_GDI_GET) || (operation == SGE_GDI_PERMCHECK) ||
       (operation == SGE_GDI_ADD && sub_command == SGE_GDI_RETURN_NEW_VERSION)) {
      if (!olpp) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_NULLPTRPASSED_S, __func__);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(false);
      }
      lXchgList(map, MA_objects, olpp);
   }

   lXchgList(map, MA_answers, alpp);
   DRETURN(true);
}

lList *sge_gdi(u_long32 target, u_long32 cmd, lList **lpp, lCondition *cp, lEnumeration *enp) {
   DENTER(GDI_LAYER);
   lList *alp = nullptr;
   lList *mal = nullptr;
   state_gdi_multi state = STATE_GDI_MULTI_INIT;

   PROF_START_MEASUREMENT(SGE_PROF_GDI);
   int id = sge_gdi_multi(&alp, SGE_GDI_SEND, target, cmd, lpp, cp, enp, &state, true);
   if (id != -1) {
      sge_gdi_wait(&mal, &state);
      gdi_extract_answer(&alp, cmd, target, id, mal, lpp);
      lFreeList(&mal);
   }
   PROF_STOP_MEASUREMENT(SGE_PROF_GDI);
   DRETURN(alp);
}

int sge_gdi_multi(lList **alpp, int mode, u_long32 target, u_long32 cmd, lList **lp, lCondition *cp, lEnumeration *enp,
                   state_gdi_multi *state, bool do_copy) {
   DENTER(GDI_LAYER);
   int ret;

   /*
    * Create a new packet (if it not already exist) and store it
    * in state_gdi_multi structure
    */
   sge_gdi_packet_class_t *packet = state->packet;
   if (packet == nullptr) {
      packet = sge_gdi_packet_create(alpp);
      state->packet = packet;
   }

   /*
    * Add a task to the packet and if it is the last task of a
    * multi GDI request (mode == SGE_GDI_SEND) then execute it
    */
   if (packet != nullptr) {
      sge_gdi_packet_append_task(packet, alpp, target, cmd, lp, nullptr, &cp, &enp, do_copy);
      ret = sge_gdi_packet_get_last_task_id(packet);
      if (mode == SGE_GDI_SEND) {
         int local_ret;

         if (component_is_qmaster_internal()) {
            local_ret = sge_gdi_packet_execute_internal(alpp, packet);
         } else {
            local_ret = sge_gdi_packet_execute_external(alpp, packet);
         }
         if (!local_ret) {
            /* answer has been written in ctx->sge_gdi_packet_execute() */
            sge_gdi_packet_free(&packet);
            state->packet = nullptr;
            ret = -1;
         }
      }
   } else {
      /* answer list has been filled by sge_gdi_packet_create() */
      ret = -1;
   }
   DRETURN(ret);
}

/****** gdi/request/sge_gdi_wait() *******************************************
*  NAME
*     sge_gdi_wait() -- wait till a GDI request is finished
*
*  SYNOPSIS
*     bool 
*     sge_gdi_wait(sge_gdi_ctx_class_t* ctx, lList **alpp, lList **malpp,
*                   state_gdi_multi *state) 
*
*  FUNCTION
*     This functions waits until a GDI multi request is handled by
*     a qmaster "worker" thread. This means that the thread executing this
*     function will block till either the GDI request is done successfully
*     or till a detailed list of error messages, describing the reason
*     why the request could not be executed, is available.
*
*     Input parameters for this function are the GDI context "ctx" 
*     and the "state" structure which has to be initialized by calling 
*     sge_gdi_multi(... mode=SGE_GDI_RECORED...) zero or multiple times
*     and sge_gdi_multi(... mode=SGE_GDI_SEND...) once.
*
*     If the function itself fails it will append answer list messages
*     to "alpp" and return this "false" as return value". Otherwise
*     the multi answer list "malpp" will be initialized, which can later 
*     on be evaluated with gdi_extract_answer(), and the function
*     will return with "true".
*
*  INPUTS
*     sge_gdi_ctx_class_t* ctx - context object 
*     lList **alpp             - answer list for this function 
*     lList **malpp            - multi answer list 
*     state_gdi_multi *state   - gdi state variable 
*
*  RESULT
*     bool - error state
*        true  - success
*        false - error
*
*  EXAMPLE
*     {
*        state_gdi_multi state = STATE_GDI_MULTI_INIT;
*        lEnumeration *what_cqueue = lWhat("%T(ALL)", CQ_Type);
*        lCondition *where_cqueue = nullptr;
*        lEnumeration *what_job = lWhat("%T(ALL)", JB_Type);
*        lCondition *where_job = nullptr;
*        lList *local_answer_list = nullptr;
*        int cqueue_request_id;
*        int job_request_id;
*
*        cqueue_request_id = sge_gdi_multi(ctx, &local_answer_list, SGE_GDI_RECORD,
*                                          SGE_CQ_LIST, SGE_GDI_GET, nullptr,
*                                          where_cqueue, what_cqueue, &state, true);
*        job_request_id = sge_gdi_multi(ctx, &local_answer_list, SGE_GDI_SEND,
*                                        SGE_JB_LIST, SGE_GDI_GET, nullptr,
*                                        where_job, what_job, &state, true);
*        if (cqueue_request_id != -1 && job_request_id != -1 &&
*            !answer_list_has_error(&local_answer_list)) {
*           lList *multi_answer_list = nullptr;
*           lList *list_cqueue = nullptr;
*           lList *list_job = nullptr;
*           lList *answer_cqueue = nullptr;
*           lList *answer_job = nullptr;
*           bool local_ret;
*
*           local_ret = sge_gdi_wait(ctx, &local_answer_list, &multi_answer_list, &state);
*           gdi_extract_answer(&answer_cqueue, SGE_GDI_GET, SGE_CQ_LIST,
*                                  cqueue_request_id, multi_answer_list, &list_cqueue);
*           gdi_extract_answer(&answer_job, SGE_GDI_GET, SGE_CQ_LIST,
*                                  job_request_id, multi_answer_list, &list_job);
*
*           if (!local_ret || answer_list_has_error(&answer_cqueue) || 
*               answer_list_has_error(&answer_job) || answer_list_has_error(&local_answer_list)) {
*              ERROR("GDI multi request failed");
*           } else {
*              INFO("GDI multi request was successful");
*              INFO("got cqueue list with " sge_U32CFormat " and cqueue answer "
*                    "list with " sge_U32CFormat " elements.", sge_u32c(lGetNumberOfElem(list_cqueue)),
*                    sge_u32c(lGetNumberOfElem(answer_cqueue)));
*              INFO("got job list with " sge_U32CFormat " and job answer "
*                    "list with " sge_U32CFormat " elements.", sge_u32c(lGetNumberOfElem(list_job)),
*                    sge_u32c(lGetNumberOfElem(answer_job)));
*           }
*           lFreeList(&multi_answer_list);
*           lFreeList(&answer_cqueue);
*           lFreeList(&answer_job);
*           lFreeList(&list_cqueue);
*           lFreeList(&list_job);
*        } else {
*           ERROR("QMASTER INTERNAL MULTI GDI TEST FAILED");
*           ERROR("unable to send intern gdi request (cqueue_request_id=%d, " "job_request_id=%d", cqueue_request_id, job_request_id);
*        }
*
*        lFreeList(&local_answer_list);
*     }
*
*  NOTES
*     MT-NOTE: sge_gdi_wait() is MT safe
*
*  SEE ALSO
*     gdi/request/sge_gdi()
*     gdi/request/sge_gdi_multi()
*     gdi/request/gdi_extract_answer()
*******************************************************************************/
void
sge_gdi_wait(lList **malpp, state_gdi_multi *state) {
   DENTER(GDI_LAYER);
   sge_gdi_packet_class_t *packet = state->packet;
   state->packet = nullptr;
   if (packet != nullptr) {
      if (component_is_qmaster_internal()) {
         sge_gdi_packet_wait_for_result_internal(&packet, malpp);
      } else {
         sge_gdi_packet_wait_for_result_external(&packet, malpp);
      }
   }
   DRETURN_VOID;
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
sge_gdi_send_any_request(int synchron, u_long32 *mid, const char *rhost, const char *commproc, int id,
                          sge_pack_buffer *pb, int tag, u_long32 response_id, lList **alpp) {
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

   dump_send_info(rhost, commproc, id, ack_type, tag, mid_pointer);

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
sge_gdi_get_any_request(char *rhost, char *commproc, u_short *id, sge_pack_buffer *pb,
                         int *tag, int synchron, u_long32 for_request_mid, u_long32 *mid) {
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

   /* TODO: do trigger or not? depends on syncrhron
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
         INFO("reopen connection to %s,%s," sge_U32CFormat " (2)\n", rhost, commproc, sge_u32c(usid));
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
      dump_receive_info(&message, &sender);

      /* TODO: there are two cases for any and addressed communication partner, two functions are needed */
      if (sender != nullptr && id) {
         *id = (u_short) sender->comp_id;
      }
      if (tag) {
         *tag = (int) message->message_tag;
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
         DEBUG("received from: %s," sge_U32CFormat "\n", sender->comp_host, sge_u32c(sender->comp_id));
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

static void dump_receive_info(cl_com_message_t **message, cl_com_endpoint_t **sender) {
   DENTER(GDI_LAYER);
   if (message != nullptr && sender != nullptr && *message != nullptr && *sender != nullptr &&
       (*sender)->comp_host != nullptr && (*sender)->comp_name != nullptr) {
      char buffer[512];
      dstring ds;
      sge_dstring_init(&ds, buffer, sizeof(buffer));

      DEBUG("<<<<<<<<<<<<<<<<<<<<");
      DEBUG("gdi_rcv: received message from %s/%s/" sge_U32CFormat ": ", (*sender)->comp_host, (*sender)->comp_name, sge_u32c( (*sender)->comp_id));
      DEBUG("gdi_rcv: cl_xml_ack_type_t: %s", cl_com_get_mih_mat_string((*message)->message_mat));
      DEBUG("gdi_rcv: message tag:       %s", sge_dump_message_tag((*message)->message_tag));
      DEBUG("gdi_rcv: message id:        " sge_U32CFormat "", sge_u32c((*message)->message_id));
      DEBUG("gdi_rcv: receive time:      %s", sge_ctime64(sge_gmt32_to_gmt64((*message)->message_receive_time.tv_sec), &ds));
      DEBUG("<<<<<<<<<<<<<<<<<<<<");
   }
   DRETURN_VOID;
}

static void
dump_send_info(const char *comp_host, const char *comp_name, int comp_id, cl_xml_ack_type_t ack_type,
               unsigned long tag, unsigned long *mid) {
   char buffer[512];
   dstring ds;

   DENTER(GDI_LAYER);
   sge_dstring_init(&ds, buffer, sizeof(buffer));

   if (comp_host != nullptr && comp_name != nullptr) {
      DEBUG(">>>>>>>>>>>>>>>>>>>>");
      DEBUG("gdi_snd: sending message to %s/%s/" sge_U32CFormat ": ", (char *) comp_host, comp_name, sge_u32c(comp_id));
      DEBUG("gdi_snd: cl_xml_ack_type_t: %s", cl_com_get_mih_mat_string(ack_type));
      DEBUG("gdi_snd: message tag:       %s", sge_dump_message_tag(tag));
      if (mid) {
         DEBUG("gdi_snd: message id:        " sge_U32CFormat "", sge_u32c(*mid));
      } else {
         DEBUG("gdi_snd: message id:        not handled by caller");
      }
      DEBUG("gdi_snd: send time:         %s", sge_ctime64(0, &ds));
      DEBUG(">>>>>>>>>>>>>>>>>>>>");
   } else {
      DEBUG(">>>>>>>>>>>>>>>>>>>>");
      DEBUG("gdi_snd: some parameters are not set");
      DEBUG(">>>>>>>>>>>>>>>>>>>>");
   }
   DRETURN_VOID;
}

/*
** NAME
**   gdi_tsm   - trigger scheduler monitoring 
** PARAMETER
**   schedd_name   - scheduler name  - ignored!
**   cell          - ignored!
** RETURN
**   answer list 
** EXTERNAL
**
** DESCRIPTION
**
** NOTES
**    MT-NOTE: gdi_tsm() is MT safe (assumptions)
*/
lList *gdi_tsm() {
   DENTER(GDI_LAYER);
   lList *alp = sge_gdi(SGE_SC_LIST, SGE_GDI_TRIGGER, nullptr, nullptr, nullptr);
   DRETURN(alp);
}

/*
** NAME
**   gdi_kill  - send shutdown/kill request to scheduler, master, execds 
** PARAMETER
**   id_list     - id list, EH_Type or EV_Type
**   cell          - cell, ignored!!!
**   option_flags  - 0
**   action_flag   - combination of MASTER_KILL, SCHEDD_KILL, EXECD_KILL, 
**                                       JOB_KILL 
** RETURN
**   answer list
** EXTERNAL
**
** DESCRIPTION
**
** NOTES
**    MT-NOTE: gdi_kill() is MT safe (assumptions)
*/
lList *gdi_kill(lList *id_list, u_long32 action_flag) {
   DENTER(GDI_LAYER);
   bool id_list_created = false;
   lList *alp = lCreateList("answer", AN_Type);

   if (action_flag & MASTER_KILL) {
      lList *tmpalp = sge_gdi(SGE_MASTER_EVENT, SGE_GDI_TRIGGER, nullptr, nullptr, nullptr);
      lAddList(alp, &tmpalp);
   }

   if (action_flag & SCHEDD_KILL) {
      char buffer[10];

      snprintf(buffer, sizeof(buffer), "%d", EV_ID_SCHEDD);
      id_list = lCreateList("kill scheduler", ID_Type);
      id_list_created = true;
      lAddElemStr(&id_list, ID_str, buffer, ID_Type);
      lList *tmpalp = sge_gdi(SGE_EV_LIST, SGE_GDI_TRIGGER, &id_list, nullptr, nullptr);
      lAddList(alp, &tmpalp);
   }

   if (action_flag & THREAD_START) {
      lList *tmpalp = sge_gdi(SGE_DUMMY_LIST, SGE_GDI_TRIGGER, &id_list, nullptr, nullptr);
      lAddList(alp, &tmpalp);
   }

   if (action_flag & EVENTCLIENT_KILL) {
      if (id_list == nullptr) {
         char buffer[10];
         snprintf(buffer, sizeof(buffer), "%d", EV_ID_ANY);
         id_list = lCreateList("kill all event clients", ID_Type);
         id_list_created = true;
         lAddElemStr(&id_list, ID_str, buffer, ID_Type);
      }
      lList *tmpalp = sge_gdi(SGE_EV_LIST, SGE_GDI_TRIGGER, &id_list, nullptr, nullptr);
      lAddList(alp, &tmpalp);
   }

   if ((action_flag & EXECD_KILL) || (action_flag & JOB_KILL)) {
      lListElem *hlep;
      const lListElem *hep;
      lList *hlp = nullptr;
      if (id_list != nullptr) {
         /*
         ** we have to convert the EH_Type to ID_Type
         ** It would be better to change the call to use ID_Type!
         */
         for_each_ep(hep, id_list) {
            hlep = lAddElemStr(&hlp, ID_str, lGetHost(hep, EH_name), ID_Type);
            lSetUlong(hlep, ID_force, (action_flag & JOB_KILL) ? 1 : 0);
         }
      } else {
         hlp = lCreateList("kill all hosts", ID_Type);
         hlep = lCreateElem(ID_Type);
         lSetString(hlep, ID_str, nullptr);
         lSetUlong(hlep, ID_force, (action_flag & JOB_KILL) ? 1 : 0);
         lAppendElem(hlp, hlep);
      }
      lList *tmpalp = sge_gdi(SGE_EH_LIST, SGE_GDI_TRIGGER, &hlp, nullptr, nullptr);
      lAddList(alp, &tmpalp);
      lFreeList(&hlp);
   }

   if (id_list_created) {
      lFreeList(&id_list);
   }

   DRETURN(alp);
}

/****** gdi/sge/sge_gdi_get_permission() **********************************
*
*  NAME
*     sge_gdi_get_permission() -- check permissions of gdi request
*
*  SYNOPSIS
*     int sge_gdi_get_permission(int option);
*
*  FUNCTION
*     This function asks the qmaster for the permission (PERM_Type) 
*     list. The option flag specifies which right should be checked. 
*     It can be MANAGER_CHECK or/and OPERATOR_CHECK at this time. If 
*     the caller has access the function returns true.
* 
*  INPUTS
*     int option - check flag (MANAGER_CHECK or OPERATOR_CHECK)
*
*  RESULT
*     bool true if caller has the right, false if not (false if qmaster 
*     not reachable)
* 
*  SEE ALSO
*     gdilib/sge_gdi_get_mapping_name()
*     gdilib/PERM_LOWERBOUND
******************************************************************************/
bool
sge_gdi_get_permission(lList **alpp, bool *is_manager, bool *is_operator,
                       bool *is_admin_host, bool *is_submit_host) {
   DENTER(TOP_LAYER);

   // fetch permissions for current user and host from qmaster
   lList *permission_list = nullptr;
   lList *alp = sge_gdi(SGE_DUMMY_LIST, SGE_GDI_PERMCHECK, &permission_list, nullptr, nullptr);
   if (permission_list == nullptr || lGetNumberOfElem(permission_list) != 1) {
      answer_list_append_list(alpp, &alp);
      lFreeList(&permission_list);
      DRETURN(false);
   }

   // prepare return values
   const lListElem *perm = lFirst(permission_list);
   if (is_manager != nullptr) {
      *is_manager = lGetBool(perm, PERM_is_manager);
      DPRINTF("is_manager: %s\n", *is_manager ? "true" : "false");
   }
   if (is_operator != nullptr) {
      *is_operator = lGetBool(perm, PERM_is_operator);
      DPRINTF("is_operator: %s\n", *is_manager ? "true" : "false");
   }
   if (is_admin_host != nullptr) {
      *is_admin_host = lGetBool(perm, PERM_is_admin_host);
      DPRINTF("is_admin_host: %s\n", *is_admin_host ? "true" : "false");
   }
   if (is_submit_host != nullptr) {
      *is_submit_host = lGetBool(perm, PERM_is_submit_host);
      DPRINTF("is_submit_host: %s\n", *is_submit_host ? "true" : "false");
   }

   lFreeList(&permission_list);
   lFreeList(&alp);
   DRETURN(true);
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
gdi_send_message_pb(int synchron, const char *tocomproc, int toid, const char *tohost,
                     int tag, sge_pack_buffer *pb, u_long32 *mid) {
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

/************************************************************
   TODO: rewrite this function
   COMMLIB/SECURITY WRAPPERS
   FIXME: FUNCTIONPOINTERS SHOULD BE SET IN sge_security_initialize !!!

   Test dlopen functionality, stub libs or check if openssl calls can be added 
   without infringing a copyright

   NOTES
      MT-NOTE: gdi_send_message() is MT safe (assumptions)
*************************************************************/
static int
gdi_send_message(int synchron, const char *tocomproc, int toid, const char *tohost, int tag,
                  char **buffer, int buflen, u_long32 *mid) {
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


/* 
 *  TODO: rewrite this function
 *  NOTES
 *     MT-NOTE: gdi_receive_message() is MT safe (major assumptions!)
 *
 */
int
gdi_receive_message(char *fromcommproc, u_short *fromid, char *fromhost,
                     int *tag, char **buffer, u_long32 *buflen, int synchron) {

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
         INFO("reopen connection to %s,%s," sge_U32CFormat " (1)\n", fromhost, fromcommproc, sge_u32c( *fromid));
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
         *tag = (int) message->message_tag;
      }

      if (sender != nullptr) {
         DEBUG("received from: %s," sge_U32CFormat "\n", sender->comp_host, sge_u32c(sender->comp_id));
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


/*-------------------------------------------------------------------------*
 * NAME
 *   get_configuration - retrieves configuration from qmaster
 * PARAMETER
 *   config_name       - name of local configuration or "global",
 *                       name is being resolved before action
 *   gepp              - pointer to list element containing global
 *                       configuration, CONF_Type, should point to nullptr
 *                       or otherwise will be freed
 *   lepp              - pointer to list element containing local configuration
 *                       by name given by config_name, can be nullptr if global
 *                       configuration is requested, CONF_Type, should point
 *                       to nullptr or otherwise will be freed
 * RETURN
 *    0   on success
 *   -1   nullptr pointer received
 *   -2   error resolving host
 *   -3   invalid nullptr pointer received for local configuration
 *   -4   request to qmaster failed
 *   -5   there is no global configuration
 *   -6   endpoint not unique
 *   -7   no permission to get configuration
 *   -8   access denied error on commlib layer
 * EXTERNAL
 *
 * DESCRIPTION
 *   retrieves a configuration from the qmaster. If the configuration
 *   "global" is requested, then this function requests only this one.
 *   If not, both the global configuration and the requested local
 *   configuration are retrieved.
 *   This function was introduced to make execution hosts independent
 *   of being able to mount the local_conf directory.
 *-------------------------------------------------------------------------*/
int
gdi_get_configuration(const char *config_name, lListElem **gepp, lListElem **lepp) {
   DENTER(GDI_LAYER);
   lCondition *where;
   lEnumeration *what;
   lList *alp = nullptr;
   lList *lp = nullptr;
   u_long32 is_global_requested = 0;
   int ret;
   lListElem *hep = nullptr;
   int success;
   static int already_logged = 0;
   u_long32 status;
   u_long32 me = component_get_component_id();

   if (config_name == nullptr || gepp == nullptr) {
      DRETURN(-1);
   }

   /* free elements referenced in gepp and lepp - we will overwrite them */
   if (*gepp != nullptr) {
      lFreeElem(gepp);
   }
   if (lepp != nullptr && *lepp) {
      lFreeElem(lepp);
   }

   /* resolve hostname, unless the global config is requested */
   if (!strcasecmp(config_name, "global")) {
      is_global_requested = 1;
   } else {
      hep = lCreateElem(EH_Type);
      lSetHost(hep, EH_name, config_name);

      ret = sge_resolve_host(hep, EH_name);

      if (ret != CL_RETVAL_OK) {
         DPRINTF("get_configuration: error %d resolving host %s: %s\n", ret, config_name, cl_get_error_text(ret));
         lFreeElem(&hep);
         ERROR(MSG_SGETEXT_CANTRESOLVEHOST_S, config_name);
         DRETURN(-2);
      }
      DPRINTF("get_configuration: unique for %s: %s\n", config_name, lGetHost(hep, EH_name));

      if (sge_get_com_error_flag(me, SGE_COM_ACCESS_DENIED, false)) {
         lFreeElem(&hep);
         DRETURN(-8);
      }
      if (sge_get_com_error_flag(me, SGE_COM_ENDPOINT_NOT_UNIQUE, false)) {
         lFreeElem(&hep);
         DRETURN(-6);
      }
   }

   if (is_global_requested == 0 && !lepp) {
      ERROR(SFNMAX, MSG_NULLPOINTER);
      lFreeElem(&hep);
      DRETURN(-3);
   }

   /* query configuration from sge_qmaster via gdi request */
   if (is_global_requested != 0) {
      /*
       * they might otherwise send global twice
       */
      where = lWhere("%T(%I c= %s)", CONF_Type, CONF_name, SGE_GLOBAL_NAME);
      DPRINTF("requesting global\n");
   } else {
      where = lWhere("%T(%I c= %s || %I h= %s)", CONF_Type, CONF_name, SGE_GLOBAL_NAME, CONF_name,
                     lGetHost(hep, EH_name));
      DPRINTF("requesting global and %s\n", lGetHost(hep, EH_name));
   }
   what = lWhat("%T(ALL)", CONF_Type);
   alp = sge_gdi(SGE_CONF_LIST, SGE_GDI_GET, &lp, where, what);

   lFreeWhat(&what);
   lFreeWhere(&where);

   /* in case the gdi request failed: error handling & return */
   success = ((status = lGetUlong(lFirst(alp), AN_status)) == STATUS_OK);
   if (!success) {
      if (!already_logged) {
         ERROR(MSG_CONF_GETCONF_S, lGetString(lFirst(alp), AN_text));
         already_logged = 1;
      }

      lFreeList(&alp);
      lFreeList(&lp);
      lFreeElem(&hep);
      DRETURN((status != STATUS_EDENIED2HOST) ? -4 : -7);
   }
   lFreeList(&alp);

   /* we didn't get the correct number of configurations? */
   if (lGetNumberOfElem(lp) > (2 - is_global_requested)) {
      WARNING(MSG_CONF_REQCONF_II, (int) (2 - is_global_requested), lGetNumberOfElem(lp));
   }

   /* we did not get the global configuration? */
   if (!(*gepp = lGetElemHostRW(lp, CONF_name, SGE_GLOBAL_NAME))) {
      ERROR(SFNMAX, MSG_CONF_NOGLOBAL);
      lFreeList(&lp);
      lFreeElem(&hep);
      DRETURN(-5);
   }
   lDechainElem(lp, *gepp);

   /* if we requested the local configuration but there is none,
    * print a warning
    */
   if (is_global_requested == 0) {
      if (!(*lepp = lGetElemHostRW(lp, CONF_name, lGetHost(hep, EH_name)))) {
         if (*gepp) {
            INFO(MSG_CONF_NOLOCAL_S, lGetHost(hep, EH_name));
         }
         lFreeList(&lp);
         lFreeElem(&hep);
         already_logged = 0;
         DRETURN(0);
      }
      lDechainElem(lp, *lepp);
   }

   lFreeElem(&hep);
   lFreeList(&lp);
   already_logged = 0;
   DRETURN(0);
}


int gdi_wait_for_conf(lList **conf_list) {
   lListElem *global = nullptr;
   lListElem *local = nullptr;
   int ret_val;
   int ret;
   static u_long64 last_qmaster_file_read = 0;
   const char *qualified_hostname = component_get_qualified_hostname();
   const char *cell_root = bootstrap_get_cell_root();
   u_long32 progid = component_get_component_id();

   /* TODO: move this function to execd */
   DENTER(GDI_LAYER);
   /*
    * for better performance retrieve 2 configurations
    * in one gdi call
    */
   DPRINTF("qualified hostname: %s\n", qualified_hostname);

   while ((ret = gdi_get_configuration(qualified_hostname, &global, &local))) {
      if (ret == -6 || ret == -7) {
         /* confict: endpoint not unique or no permission to get config */
         DRETURN(-1);
      }

      if (ret == -8) {
         /* access denied */
         sge_get_com_error_flag(progid, SGE_COM_ACCESS_DENIED, true);
         sleep(30);
      }

      DTRACE;
      cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
      ret_val = cl_commlib_trigger(handle, 1);
      switch (ret_val) {
         case CL_RETVAL_SELECT_TIMEOUT:
            sleep(1);  /* If we could not establish the connection */
            break;
         case CL_RETVAL_OK:
            break;
         default:
            sleep(1);  /* for other errors */
            break;
      }

      u_long64 now = sge_get_gmt64();
      if (now - last_qmaster_file_read >= sge_gmt32_to_gmt64(30)) {
         gdi_get_act_master_host(true);
         DPRINTF("re-read actual qmaster file\n");
         last_qmaster_file_read = now;
      }
   }

   ret = merge_configuration(nullptr, progid, cell_root, global, local, nullptr);
   if (ret) {
      DPRINTF("Error %d merging configuration \"%s\"\n", ret, qualified_hostname);
   }

   /*
    * we don't keep all information, just the name and the version
    * the entries are freed
    */
   lSetList(global, CONF_entries, nullptr);
   lSetList(local, CONF_entries, nullptr);
   lFreeList(conf_list);
   *conf_list = lCreateList("config list", CONF_Type);
   lAppendElem(*conf_list, global);
   lAppendElem(*conf_list, local);
   DRETURN(0);
}

/*-------------------------------------------------------------------------*
 * NAME
 *   get_merged_conf - requests new configuration set from master
 * RETURN
 *   -1      - could not get configuration from qmaster
 *   -2      - could not merge global and local configuration
 * EXTERNAL
 *
 *-------------------------------------------------------------------------*/
int gdi_get_merged_configuration(lList **conf_list) {
   lListElem *global = nullptr;
   lListElem *local = nullptr;
   const char *qualified_hostname = component_get_qualified_hostname();
   const char *cell_root = bootstrap_get_cell_root();
   u_long32 progid = component_get_component_id();
   int ret;

   DENTER(GDI_LAYER);

   DPRINTF("qualified hostname: %s\n", qualified_hostname);
   ret = gdi_get_configuration(qualified_hostname, &global, &local);
   if (ret) {
      ERROR(MSG_CONF_NOREADCONF_IS, ret, qualified_hostname);
      lFreeElem(&global);
      lFreeElem(&local);
      DRETURN(-1);
   }

   ret = merge_configuration(nullptr, progid, cell_root, global, local, nullptr);
   if (ret) {
      ERROR(MSG_CONF_NOMERGECONF_IS, ret, qualified_hostname);
      lFreeElem(&global);
      lFreeElem(&local);
      DRETURN(-2);
   }
   /*
    * we don't keep all information, just the name and the version
    * the entries are freed
    */
   lSetList(global, CONF_entries, nullptr);
   lSetList(local, CONF_entries, nullptr);

   lFreeList(conf_list);
   *conf_list = lCreateList("config list", CONF_Type);
   lAppendElem(*conf_list, global);
   lAppendElem(*conf_list, local);

   DRETURN(0);
}


void gdi_default_exit_func(int i) {
   sge_security_exit(i);
   cl_com_cleanup_commlib();
}



/****** sgeobj/sge_report/report_list_send() ******************************************
*  NAME
*     report_list_send() -- Send a list of reports.
*
*  SYNOPSIS
*     int report_list_send(const lList *rlp, const char *rhost,
*                          const char *commproc, int id,
*                          int synchron, u_long32 *mid)
*
*  FUNCTION
*     Send a list of reports.
*
*  INPUTS
*     const lList *rlp     - REP_Type list
*     const char *rhost    - Hostname
*     const char *commproc - Component name
*     int id               - Component id
*     int synchron         - true or false
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Unexpected error
*        -2 - No memory
*        -3 - Format error
*        other - see sge_send_any_request()
*
*  NOTES
*     MT-NOTE: report_list_send() is not MT safe (assumptions)
*******************************************************************************/
int report_list_send(const lList *rlp, const char *rhost, const char *commproc, int id, int synchron) {
   sge_pack_buffer pb;
   int ret;
   lList *alp = nullptr;

   DENTER(TOP_LAYER);

   /* prepare packing buffer */
   if ((ret = init_packbuffer(&pb, 1024, 0)) == PACK_SUCCESS) {
      ret = cull_pack_list(&pb, rlp);
   }

   switch (ret) {
      case PACK_SUCCESS:
         break;

      case PACK_ENOMEM:
         ERROR(MSG_GDI_REPORTNOMEMORY_I, 1024);
         clear_packbuffer(&pb);
         DRETURN(-2);

      case PACK_FORMAT:
         ERROR(SFNMAX, MSG_GDI_REPORTFORMATERROR);
         clear_packbuffer(&pb);
         DRETURN(-3);

      default:
         ERROR(SFNMAX, MSG_GDI_REPORTUNKNOWERROR);
         clear_packbuffer(&pb);
         DRETURN(-1);
   }

   ret = sge_gdi_send_any_request(synchron, nullptr, rhost, commproc, id, &pb, TAG_REPORT_REQUEST, 0, &alp);

   clear_packbuffer(&pb);
   answer_list_output(&alp);

   DRETURN(ret);
}
/************* COMMLIB HANDLERS from sge_any_request ************************/

/* setup a communication error callback and mutex for it */
static pthread_mutex_t general_communication_error_mutex = PTHREAD_MUTEX_INITIALIZER;


/* local static struct to store communication errors. The boolean
 * values com_access_denied and com_endpoint_not_unique will never be
 * restored to false again 
 */
typedef struct sge_gdi_com_error_type {
   int com_error;                        /* current commlib error */
   bool com_was_error;                    /* set if there was an communication error (but not CL_RETVAL_ACCESS_DENIED or CL_RETVAL_ENDPOINT_NOT_UNIQUE)*/
   int com_last_error;                   /* last logged commlib error */
   bool com_access_denied;                /* set when commlib reports CL_RETVAL_ACCESS_DENIED */
   int com_access_denied_counter;        /* counts access denied errors (TODO: workaround for BT: 6350264, IZ: 1893) */
   time_t com_access_denied_time;         /* timeout for counts access denied errors (TODO: workaround for BT: 6350264, IZ: 1893) */
   bool com_endpoint_not_unique;          /* set when commlib reports CL_RETVAL_ENDPOINT_NOT_UNIQUE */
   int com_endpoint_not_unique_counter;  /* counts access denied errors (TODO: workaround for BT: 6350264, IZ: 1893) */
   time_t com_endpoint_not_unique_time;   /* timeout for counts access denied errors (TODO: workaround for BT: 6350264, IZ: 1893) */
} sge_gdi_com_error_t;

static sge_gdi_com_error_t sge_gdi_communication_error = {CL_RETVAL_OK,
                                                          false,
                                                          CL_RETVAL_OK,
                                                          false, 0, 0,
                                                          false, 0, 0};


/****** sge_any_request/sge_dump_message_tag() *************************************
*  NAME
*     sge_dump_message_tag() -- get tag name string
*
*  SYNOPSIS
*     const char* sge_dump_message_tag(int tag) 
*
*  FUNCTION
*     This is a function used for getting a printable string output for the
*     different message tags.
*     (Useful for debugging)
*
*  INPUTS
*     int tag - tag value
*
*  RESULT
*     const char* - name of tag
*
*  NOTES
*     MT-NOTE: sge_dump_message_tag() is MT safe 
*******************************************************************************/
const char *sge_dump_message_tag(unsigned long tag) {
   switch (tag) {
      case TAG_NONE:
         return "TAG_NONE";
      case TAG_OLD_REQUEST:
         return "TAG_OLD_REQUEST";
      case TAG_GDI_REQUEST:
         return "TAG_GDI_REQUEST";
      case TAG_ACK_REQUEST:
         return "TAG_ACK_REQUEST";
      case TAG_REPORT_REQUEST:
         return "TAG_REPORT_REQUEST";
      case TAG_FINISH_REQUEST:
         return "TAG_FINISH_REQUEST";
      case TAG_JOB_EXECUTION:
         return "TAG_JOB_EXECUTION";
      case TAG_SLAVE_ALLOW:
         return "TAG_SLAVE_ALLOW";
      case TAG_CHANGE_TICKET:
         return "TAG_CHANGE_TICKET";
      case TAG_SIGJOB:
         return "TAG_SIGJOB";
      case TAG_SIGQUEUE:
         return "TAG_SIGQUEUE";
      case TAG_KILL_EXECD:
         return "TAG_KILL_EXECD";
      case TAG_NEW_FEATURES:
         return "TAG_NEW_FEATURES";
      case TAG_GET_NEW_CONF:
         return "TAG_GET_NEW_CONF";
      case TAG_JOB_REPORT:
         return "TAG_JOB_REPORT";
      case TAG_TASK_EXIT:
         return "TAG_TASK_EXIT";
      case TAG_TASK_TID:
         return "TAG_TASK_TID";
      case TAG_FULL_LOAD_REPORT:
         return "TAG_FULL_LOAD_REPORT";
      case TAG_EVENT_CLIENT_EXIT:
         return "TAG_EVENT_CLIENT_EXIT";
      default:
         break;
   }
   return "TAG_NOT_DEFINED";
}

/****** sge_any_request/general_communication_error() **************************
*  NAME
*     general_communication_error() -- callback for communication errors
*
*  SYNOPSIS
*     static void general_communication_error(int cl_error, 
*                                             const char* error_message) 
*
*  FUNCTION
*     This function is used by cl_com_set_error_func() to set the default
*     application error function for communication errors. On important 
*     communication errors the communication lib will call this function
*     with a corresponding error number (within application context).
*
*     This function should never block. Treat it as a kind of signal handler.
*    
*     The error_message parameter is freed by the commlib.
*
*  INPUTS
*     int cl_error              - commlib error number
*     const char* error_message - additional error text message
*
*  NOTES
*     MT-NOTE: general_communication_error() is MT safe 
*     (static struct variable "sge_gdi_communication_error" is used)
*
*
*  SEE ALSO
*     sge_any_request/sge_get_com_error_flag()
*******************************************************************************/
void
general_communication_error(const cl_application_error_list_elem_t *commlib_error) {
   DENTER(GDI_LAYER);
   if (commlib_error != nullptr) {
      struct timeval now{};
      unsigned long time_diff;

      sge_mutex_lock("general_communication_error_mutex",
                     __func__, __LINE__, &general_communication_error_mutex);

      /* save the communication error to react later */
      sge_gdi_communication_error.com_error = commlib_error->cl_error;

      switch (commlib_error->cl_error) {
         case CL_RETVAL_OK: {
            break;
         }
         case CL_RETVAL_ACCESS_DENIED: {
            if (!sge_gdi_communication_error.com_access_denied) {
               /* counts access denied errors (TODO: workaround for BT: 6350264, IZ: 1893) */
               /* increment counter only once per second and allow max CL_DEFINE_READ_TIMEOUT + 2 access denied */
               gettimeofday(&now, nullptr);
               if ((now.tv_sec - sge_gdi_communication_error.com_access_denied_time) > (3 * CL_DEFINE_READ_TIMEOUT)) {
                  sge_gdi_communication_error.com_access_denied_time = 0;
                  sge_gdi_communication_error.com_access_denied_counter = 0;
               }

               if (sge_gdi_communication_error.com_access_denied_time < now.tv_sec) {
                  if (sge_gdi_communication_error.com_access_denied_time == 0) {
                     time_diff = 1;
                  } else {
                     time_diff = now.tv_sec - sge_gdi_communication_error.com_access_denied_time;
                  }
                  sge_gdi_communication_error.com_access_denied_counter += time_diff;
                  if (sge_gdi_communication_error.com_access_denied_counter > (2 * CL_DEFINE_READ_TIMEOUT)) {
                     sge_gdi_communication_error.com_access_denied = true;
                  }
                  sge_gdi_communication_error.com_access_denied_time = now.tv_sec;
               }
            }
            break;
         }
         case CL_RETVAL_ENDPOINT_NOT_UNIQUE: {
            if (!sge_gdi_communication_error.com_endpoint_not_unique) {
               /* counts endpoint not unique errors (TODO: workaround for BT: 6350264, IZ: 1893) */
               /* increment counter only once per second and allow max CL_DEFINE_READ_TIMEOUT + 2 endpoint not unique */
               DPRINTF("got endpint not unique");
               gettimeofday(&now, nullptr);
               if ((now.tv_sec - sge_gdi_communication_error.com_endpoint_not_unique_time) >
                   (3 * CL_DEFINE_READ_TIMEOUT)) {
                  sge_gdi_communication_error.com_endpoint_not_unique_time = 0;
                  sge_gdi_communication_error.com_endpoint_not_unique_counter = 0;
               }

               if (sge_gdi_communication_error.com_endpoint_not_unique_time < now.tv_sec) {
                  if (sge_gdi_communication_error.com_endpoint_not_unique_time == 0) {
                     time_diff = 1;
                  } else {
                     time_diff = now.tv_sec - sge_gdi_communication_error.com_endpoint_not_unique_time;
                  }
                  sge_gdi_communication_error.com_endpoint_not_unique_counter += time_diff;
                  if (sge_gdi_communication_error.com_endpoint_not_unique_counter > (2 * CL_DEFINE_READ_TIMEOUT)) {
                     sge_gdi_communication_error.com_endpoint_not_unique = true;
                  }
                  sge_gdi_communication_error.com_endpoint_not_unique_time = now.tv_sec;
               }
            }
            break;
         }
         default: {
            sge_gdi_communication_error.com_was_error = true;
            break;
         }
      }


      /*
       * now log the error if not already reported the 
       * least CL_DEFINE_MESSAGE_DUP_LOG_TIMEOUT seconds
       */
      if (!commlib_error->cl_already_logged &&
          sge_gdi_communication_error.com_last_error != sge_gdi_communication_error.com_error) {

         /*  never log the same messages again and again (commlib
          *  will erase cl_already_logged flag every CL_DEFINE_MESSAGE_DUP_LOG_TIMEOUT
          *  seconds (30 seconds), so we have to save the last one!
          */
         sge_gdi_communication_error.com_last_error = sge_gdi_communication_error.com_error;

         switch (commlib_error->cl_err_type) {
            case CL_LOG_ERROR: {
               if (commlib_error->cl_info != nullptr) {
                  ERROR(MSG_GDI_GENERAL_COM_ERROR_SS, cl_get_error_text(commlib_error->cl_error), commlib_error->cl_info);
               } else {
                  ERROR(MSG_GDI_GENERAL_COM_ERROR_S, cl_get_error_text(commlib_error->cl_error));
               }
               break;
            }
            case CL_LOG_WARNING: {
               if (commlib_error->cl_info != nullptr) {
                  WARNING(MSG_GDI_GENERAL_COM_ERROR_SS, cl_get_error_text(commlib_error->cl_error), commlib_error->cl_info);
               } else {
                  WARNING(MSG_GDI_GENERAL_COM_ERROR_S, cl_get_error_text(commlib_error->cl_error));
               }
               break;
            }
            case CL_LOG_INFO: {
               if (commlib_error->cl_info != nullptr) {
                  INFO(MSG_GDI_GENERAL_COM_ERROR_SS, cl_get_error_text(commlib_error->cl_error), commlib_error->cl_info);
               } else {
                  INFO(MSG_GDI_GENERAL_COM_ERROR_S, cl_get_error_text(commlib_error->cl_error));
               }
               break;
            }
            case CL_LOG_DEBUG: {
               if (commlib_error->cl_info != nullptr) {
                  DEBUG(MSG_GDI_GENERAL_COM_ERROR_SS, cl_get_error_text(commlib_error->cl_error), commlib_error->cl_info);
               } else {
                  DEBUG(MSG_GDI_GENERAL_COM_ERROR_S, cl_get_error_text(commlib_error->cl_error));
               }
               break;
            }
            case CL_LOG_OFF: {
               break;
            }
         }
      }
      sge_mutex_unlock("general_communication_error_mutex",
                       __func__, __LINE__, &general_communication_error_mutex);
   }
   DRETURN_VOID;
}


/****** sge_any_request/sge_get_com_error_flag() *******************************
*  NAME
*     sge_get_com_error_flag() -- return gdi error flag state
*
*  SYNOPSIS
*     bool sge_get_com_error_flag(sge_gdi_stored_com_error_t error_type) 
*
*  FUNCTION
*     This function returns the error flag for the specified error type
*
*  INPUTS
*     sge_gdi_stored_com_error_t error_type - error type value
*
*  RESULT
*     bool - true: error has occured, false: error never occured
*
*  NOTES
*     MT-NOTE: sge_get_com_error_flag() is MT safe 
*
*  SEE ALSO
*     sge_any_request/general_communication_error()
*******************************************************************************/
bool sge_get_com_error_flag(u_long32 progid, sge_gdi_stored_com_error_t error_type, bool reset_error_flag) {
   DENTER(GDI_LAYER);
   bool ret_val = false;
   sge_mutex_lock("general_communication_error_mutex", __func__, __LINE__, &general_communication_error_mutex);

   /* 
    * never add a default case for that switch, because of compiler warnings
    * for un-"cased" values 
    */

   /* TODO: remove component_get_component_id()/progid cases for QMASTER and EXECD after BT: 6350264, IZ: 1893 is fixed */
   switch (error_type) {
      case SGE_COM_ACCESS_DENIED: {
         ret_val = sge_gdi_communication_error.com_access_denied;
         if (reset_error_flag) {
            sge_gdi_communication_error.com_access_denied = false;
         }
         break;
      }
      case SGE_COM_ENDPOINT_NOT_UNIQUE: {
         if (progid == QMASTER || progid == EXECD) {
            ret_val = false;
         } else {
            ret_val = sge_gdi_communication_error.com_endpoint_not_unique;
         }
         if (reset_error_flag) {
            sge_gdi_communication_error.com_endpoint_not_unique = false;
         }
         break;
      }
      case SGE_COM_WAS_COMMUNICATION_ERROR: {
         ret_val = sge_gdi_communication_error.com_was_error;
         if (reset_error_flag) {
            sge_gdi_communication_error.com_was_error = false;  /* reset error flag */
         }
      }
   }
   sge_mutex_unlock("general_communication_error_mutex", __func__, __LINE__, &general_communication_error_mutex);
   DRETURN(ret_val);
}
