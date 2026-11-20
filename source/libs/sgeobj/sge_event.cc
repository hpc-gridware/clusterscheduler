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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Rendering and verifying event and event client objects
 *
 * @see sge_event.h
 */

#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "comm/cl_communication.h" 

#include "cull/cull.h"

#include "sgeobj/sge_job.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/tom/ocs_tom_EVS.h"
#include "sgeobj/tom/ocs_tom_STN.h"

#include "msg_common.h"

/**
 * @brief Render an event for logging and for `qconf -secl`
 *
 * @param event the event to render
 * @param[out] buffer receives the text
 * @return the rendered text, i.e. the buffer's content
 *
 * @see @ref evc
 */
const char *event_text(const ocs::tom::ConstElement<ocs::tom::ET> event, dstring *buffer) {
   DENTER(TOP_LAYER);
   int n=0;
   dstring id_dstring = DSTRING_INIT;

   const uint32_t number = ocs::tom::get<ET_number>(event);
   const uint32_t type = ocs::tom::get<ET_type>(event);
   const uint32_t intkey = ocs::tom::get<ET_intkey>(event);
   const uint32_t intkey2 = ocs::tom::get<ET_intkey2>(event);
   const char *strkey = ocs::tom::get<ET_strkey>(event);
   const char *strkey2 = ocs::tom::get<ET_strkey2>(event);
   // the objects of the event; their type depends on the event type, so the list is untyped
   const ocs::tom::ConstRawList lp = ocs::tom::get<ET_new_version>(event);
   n = static_cast<int>(lp.size());

   switch (type) {

   /* -------------------- */

   /* -------------------- */
   case sgeE_CALENDAR_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "CALENDAR", n);
      break;
   case sgeE_CALENDAR_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "CALENDAR", strkey);
      break;
   case sgeE_CALENDAR_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "CALENDAR", strkey);
      break;
   case sgeE_CALENDAR_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "CALENDAR", strkey);
      break;

   /* -------------------- */
   case sgeE_CKPT_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI,  number, "CKPT", n);
      break;
   case sgeE_CKPT_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "CKPT", strkey);
      break;
   case sgeE_CKPT_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "CKPT", strkey);
      break;
   case sgeE_CKPT_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "CKPT", strkey);
      break;

   /* -------------------- */
   case sgeE_CONFIG_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "CONFIG", n);
      break;
   case sgeE_CONFIG_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "CONFIG", strkey);
      break;
   case sgeE_CONFIG_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "CONFIG", strkey);
      break;
   case sgeE_CONFIG_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "CONFIG", strkey);
      break;

   /* -------------------- */
   case sgeE_EXECHOST_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "EXECHOST", n);
      break;
   case sgeE_EXECHOST_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "EXECHOST", strkey);
      break;
   case sgeE_EXECHOST_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "EXECHOST", strkey);
      break;
   case sgeE_EXECHOST_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "EXECHOST", strkey);
      break;

   /* -------------------- */
   case sgeE_JATASK_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "JATASK",
                          job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JATASK_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "JATASK",
                          job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JATASK_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "JATASK",
                          job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;

   /* -------------------- */
   case sgeE_PETASK_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "PETASK",
                          job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_PETASK_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "PETASK",
                          job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
#if 0      
   /* JG: we'll have it soon ;-) */
   case sgeE_PETASK_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "PETASK",
                          job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
#endif

   /* -------------------- */
   case sgeE_JOB_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "JOB", n);
      break;
   case sgeE_JOB_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "JOB",
                          job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JOB_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "JOB",
                          job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JOB_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "JOB",
                          job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JOB_USAGE:
      sge_dstring_sprintf(buffer, MSG_EVENT_JOBXUSAGE_US, 
         number, job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JOB_FINAL_USAGE:
      sge_dstring_sprintf(buffer, MSG_EVENT_JOBXFINALUSAGE_US, 
         number, job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;

   case sgeE_JOB_FINISH:
      sge_dstring_sprintf(buffer, MSG_EVENT_JOBXFINISH_US, 
         number, job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;

   /* -------------------- */
   case sgeE_JOB_SCHEDD_INFO_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "JOB_SCHEDD_INFO", n);
      break;
   case sgeE_JOB_SCHEDD_INFO_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "JOB_SCHEDD_INFO",
                          job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JOB_SCHEDD_INFO_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "JOB_SCHEDD_INFO",
                          job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;
   case sgeE_JOB_SCHEDD_INFO_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "JOB_SCHEDD_INFO",
                          job_get_id_string(intkey, intkey2, strkey, &id_dstring));
      break;

   /* -------------------- */
   case sgeE_NEW_SHARETREE:
      {
         // a sharetree event carries the tree, STN
         const ocs::tom::ConstList<ocs::tom::STN> tree(lp);
         sge_dstring_sprintf(buffer, MSG_EVENT_SHARETREEXNODESYLEAFS_UII, number,
            tree.count_nodes<STN_children>(),
            tree.count_leaves<STN_children>());
      }
      break;

   /* -------------------- */
   case sgeE_PE_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "PE", n);
      break;
   case sgeE_PE_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "PE", strkey);
      break;
   case sgeE_PE_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "PE", strkey);
      break;
   case sgeE_PE_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "PE", strkey);
      break;

   /* -------------------- */
   case sgeE_PROJECT_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "PROJECT", n);
      break;
   case sgeE_PROJECT_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "PROJECT", strkey);
      break;
   case sgeE_PROJECT_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "PROJECT", strkey);
      break;
   case sgeE_PROJECT_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "PROJECT", strkey);
      break;

   /* -------------------- */
   case sgeE_QMASTER_GOES_DOWN:
      sge_dstring_sprintf(buffer, MSG_EVENT_MESSAGE_US, number, "QMASTER GOES DOWN");
      break;

   /* -------------------- */
   case sgeE_ACK_TIMEOUT:
      sge_dstring_sprintf(buffer, MSG_EVENT_MESSAGE_US, number, "ACK TIMEOUT");
      break;

   /* -------------------- */
   case sgeE_CQUEUE_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "CLUSTER QUEUE", n);
      break;
   case sgeE_CQUEUE_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "CLUSTER QUEUE", strkey);
      break;
   case sgeE_CQUEUE_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "CLUSTER QUEUE", strkey);
      break;
   case sgeE_CQUEUE_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "CLUSTER QUEUE", strkey);
      break;

   /* -------------------- */
   case sgeE_QINSTANCE_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDQINSTANCE_USS, number, strkey, strkey2);
      break;
   case sgeE_QINSTANCE_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELQINSTANCE_USS, number, strkey, strkey2);
      break;
   case sgeE_QINSTANCE_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODQINSTANCE_USS, number, strkey, strkey2);
      break;
   case sgeE_QINSTANCE_USOS:
      sge_dstring_sprintf(buffer, MSG_EVENT_UNSUSPENDQUEUEXONSUBORDINATE_US, number, strkey);
      break;
   case sgeE_QINSTANCE_SOS:
      sge_dstring_sprintf(buffer, MSG_EVENT_SUSPENDQUEUEXONSUBORDINATE_US, number, strkey);
      break;

   /* -------------------- */
   case sgeE_SCHED_CONF:
      sge_dstring_sprintf(buffer, MSG_EVENT_MESSAGE_US, number, "SCHEDULER CONFIG");
      break;

   /* -------------------- */
   case sgeE_SCHEDDMONITOR:
      sge_dstring_sprintf(buffer, MSG_EVENT_MESSAGE_US, number, "TRIGGER SCHEDULER MONITORING");
      break;

   /* -------------------- */
   case sgeE_SHUTDOWN:
      sge_dstring_sprintf(buffer, MSG_EVENT_MESSAGE_US, number, "SHUTDOWN");
      break;

   /* -------------------- */

   /* -------------------- */
   case sgeE_USER_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "USER", n);
      break;
   case sgeE_USER_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "USER", strkey);
      break;
   case sgeE_USER_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "USER", strkey);
      break;
   case sgeE_USER_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "USER", strkey);
      break;

   /* -------------------- */
   case sgeE_USERSET_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "USER SET", n);
      break;
   case sgeE_USERSET_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "USER SET", strkey);
      break;
   case sgeE_USERSET_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "USER SET", strkey);
      break;
   case sgeE_USERSET_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "USER SET", strkey);
      break;

   /* -------------------- */
   case sgeE_HGROUP_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "HOST GROUP", n);
      break;
   case sgeE_HGROUP_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "HOST GROUP", strkey);
      break;
   case sgeE_HGROUP_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "HOST GROUP", strkey);
      break;
   case sgeE_HGROUP_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "HOST GROUP", strkey);
      break;

   /* -------------------- */
   case sgeE_CENTRY_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "COMPLEX ENTRY", n);
      break;
   case sgeE_CENTRY_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "COMPLEX ENTRY", strkey);
      break;
   case sgeE_CENTRY_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "COMPLEX ENTRY", strkey);
      break;
   case sgeE_CENTRY_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "COMPLEX ENTRY", strkey);
      break;

   /* -------------------- */
   case sgeE_RQS_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "RESOURCE QUOTA", n);
      break;
   case sgeE_RQS_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "RESOURCE QUOTA", strkey);
      break;
   case sgeE_RQS_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "RESOURCE QUOTA", strkey);
      break;
   case sgeE_RQS_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "RESOURCE QUOTA", strkey);
      break;

   /* -------------------- */
   case sgeE_AR_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "ADVANCE RESERVATION", n);
      break;
   case sgeE_AR_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USU, number, "ADVANCE RESERVATION", intkey);
      break;
   case sgeE_AR_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USU, number, "ADVANCE RESERVATION", intkey);
      break;
   case sgeE_AR_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USU, number, "ADVANCE RESERVATION", intkey);
      break;

   /* -------------------- */
   case sgeE_CATEGORY_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "CATEGORY", n);
      break;
   case sgeE_CATEGORY_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USU, number, "CATEGORY", intkey);
      break;
   case sgeE_CATEGORY_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USU, number, "CATEGORY", intkey);
      break;
   case sgeE_CATEGORY_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USU, number, "CATEGORY", intkey);
      break;

   /* -------------------- */
   case sgeE_RL_LIST:
      sge_dstring_sprintf(buffer, MSG_EVENT_OBJECTLISTXELEMENTS_USI, number, "ROLE", n);
      break;
   case sgeE_RL_ADD:
      sge_dstring_sprintf(buffer, MSG_EVENT_ADDOBJECTX_USS, number, "ROLE", strkey);
      break;
   case sgeE_RL_DEL:
      sge_dstring_sprintf(buffer, MSG_EVENT_DELOBJECTX_USS, number, "ROLE", strkey);
      break;
   case sgeE_RL_MOD:
      sge_dstring_sprintf(buffer, MSG_EVENT_MODOBJECTX_USS, number, "ROLE", strkey);
      break;

   /* -------------------- */
   default:
      sge_dstring_sprintf(buffer, MSG_EVENT_MESSAGE_US, number, "????????");
      break;
   }

   uint64_t unique_id = ocs::tom::get<ET_unique_id>(event);
   sge_dstring_sprintf_append(buffer, " (# " sge_u64 ")", unique_id);

   sge_dstring_free(&id_dstring);

   DRETURN(sge_dstring_get_string(buffer));
}


/**
 * @brief Verify the subscription list of an event client
 *
 * @param event_client the event client registration
 * @param[out] answer_list receives the reason it was rejected
 * @param d_time the event delivery interval, unused
 * @return true when every subscribed event id is valid
 */
static bool event_client_verify_subscription(const ocs::tom::ConstElement<ocs::tom::EV> event_client,
                                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                             [[maybe_unused]] const int d_time) {
   DENTER(TOP_LAYER);
   bool ret = true;

   for (const auto ep : ocs::tom::get<EV_subscribed>(event_client)) {
      const uint32_t id = ocs::tom::get<EVS_id>(ep);
      if (id <= sgeE_ALL_EVENTS || id >= sgeE_EVENTSIZE) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_EVENT_INVALIDEVENT);
         ret = false;
         break;
      }
   }

   /* TODO: verify the where and what elements */

   DRETURN(ret);
}

/**
 * @brief Verify an event client object
 *
 * Verifies, if an incoming event client object (new event client registration
 * through a GDI_ADD operation or event client modification through GDI_MOD
 * operation is correct.
 * We do the following verifications:
 *    - EV_id correct:
 *       - add request usually may only request dynamic event client id,
 *         if a special id is requested, we must be on local host and be
 *         admin user or root.
 *    - EV_name (valid string, limited length)
 *    - EV_d_time (valid delivery interval)
 *    - EV_subscribed (valid subscription list)
 *    - EV_busy_handling (valid busy handling)
 *    - EV_session (valid string, limited length)
 * No verification will be done
 *    - EV_host (is always overwritten by qmaster code)
 *    - EV_commproc (comes from commlib)
 *    - EV_commid (comes from commlib)
 *    - EV_uid (is always overwritten  by qmaster code)
 *    - EV_last_heard_from (only used by qmaster)
 *    - EV_last_send_time (only used by qmaster)
 *    - EV_next_send_time (only used by qmaster)
 *    - EV_sub_array (only used by qmaster)
 *    - EV_changed (?)
 *    - EV_next_number (?)
 *    - EV_state (?)
 *
 * @param event_client the registration to verify
 * @param[out] answer_list receives the reason it was rejected
 * @param add is this an add request (or mod)?
 *
 * @return true when the registration is acceptable
 *
 * @note MT-NOTE: event_client_verify() is MT safe
 */
bool event_client_verify(const ocs::tom::ConstElement<ocs::tom::EV> event_client,
                         ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const bool add) {
   DENTER(TOP_LAYER);

   bool ret = true;
   const char *str;
   u_long d_time = 0;
  
   if (!event_client) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_NULLELEMENTPASSEDTO_S, __func__);
      DTRACE;
      ret = false;
   }

#if 1
   if (ret) {
      if (!object_verify_cull(event_client, ocs::tom::EV::descriptor.e)) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_OBJECT_STRUCTURE_ERROR);
         DTRACE;
         ret = false;
      }
   }
#endif   

   if (ret) {
      /* get the event delivery time - we'll need it in later checks */
      d_time = ocs::tom::get<EV_d_time>(event_client);

      /* 
       * EV_name has to be a valid string.
       * TODO: Is verify_str_key too restrictive? Does drmaa allow to set the name?
       */
      str = ocs::tom::get<EV_name>(event_client);
      if (str == nullptr ||
         verify_str_key(
            answer_list, str, MAX_VERIFY_STRING,
            ocs::tom::FieldTraits<EV_name>::info.name, KEY_TABLE) != STATUS_OK) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_EVENT_INVALIDNAME);
         DTRACE;
         ret = false;
         DPRINTF("EV name false\n");
      }
   }

   /* 
    * Verify the EV_id:
    * Add requests may only contain EV_ID_ANY or a special id.
    * If a special id is requested, it must come from admin/root
    */
   if (ret) {
#if 1      
      /* TODO: jgdi does not work with the check fix needed */
      uint32_t id = ocs::tom::get<EV_id>(event_client);
      if (add && id >= EV_ID_FIRST_DYNAMIC) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_EVENT_INVALIDID);
         DTRACE;
         ret = false;
         DPRINTF("EV_id false: " sge_u32 "\n", id);
      }
#endif
   }

   /* Event delivery time may not be gt commlib timeout */
   if (ret) {
      if (d_time < 1 || d_time > CL_DEFINE_CLIENT_CONNECTION_LIFETIME-5) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_EVENT_INVALIDDTIME_II, d_time,
                                 CL_DEFINE_CLIENT_CONNECTION_LIFETIME-5);
         ret = false;
         DPRINTF("d_time false\n");
      }
   }

   /* subscription */
   if (ret) {
      ret = event_client_verify_subscription(event_client, answer_list, (int)d_time);
   }

   /* busy handling comes from an enum with defined set of values */
   if (ret) {
      uint32_t busy = ocs::tom::get<EV_busy_handling>(event_client);
      if (busy != EV_BUSY_NO_HANDLING && busy != EV_BUSY_UNTIL_ACK &&
          busy != EV_BUSY_UNTIL_RELEASED) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_EVENT_INVALIDBUSYHANDLING);
         ret = false;
      }
   }

   /* verify session key. TODO: verify_str_key too restrictive? */
   if (ret) {
      const char *session = ocs::tom::get<EV_session>(event_client);
      if (session != nullptr) {
         if (verify_str_key(
         answer_list, session, MAX_VERIFY_STRING,
         "session key", KEY_TABLE) != STATUS_OK) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_EVENT_INVALIDSESSIONKEY);
            ret = false;
         }
      }
   }

   /* disallow an EV_update_function from event_client request */
   if (ret) {
      if (ocs::tom::get<EV_update_function>(event_client) != nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, MSG_EVENT_INVALIDUPDATEFUNCTION);
         ret = false;
      }
   }

   DRETURN(ret);
}
