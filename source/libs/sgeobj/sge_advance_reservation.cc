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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Advance reservations: resources held for a time window
 *
 * A reservation is granted once and then moves through the states in
 * @ref ar_state_t as its start and end times pass; jobs submitted into it run
 * against the resources it already holds.
 *
 * @see sge_advance_reservation.h
 */

#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "msg_qmaster.h"
#include "uti/sge.h"

/**
 * @brief Locate a advance reservation by id
 *
 * This function returns a ar object with the selected id from the
 * given list.
 *
 * @param ar_list list to be searched in
 * @param ar_id id of interest
 *
 * @return if found the reference to the ar object, else nullptr
 *
 * @note MT-NOTE: ar_list_locate() is MT safe
 */
lListElem *ar_list_locate(const lList *ar_list, uint32_t ar_id)
{
   lListElem *ep = nullptr;

   DENTER(TOP_LAYER);

   ep = lGetElemUlongRW(ar_list, AR_id, ar_id);

   DRETURN(ep);
}

/**
 * @brief Validate a advance reservation
 *
 * Ensures a new ar has valid start and end times
 *
 * @param ar the ar to check
 * @param alpp answer list pointer
 * @param in_master are we in qmaster?
 * @param is_spool do we validate for spooling?
 * @param master_cqueue_list the cluster queues the reservation may name
 * @param master_hgroup_list the host groups it may name
 * @param master_centry_list the complex entries its requests may name
 * @param master_ckpt_list the checkpointing environments it may name
 * @param master_pe_list the parallel environments it may name
 * @param master_userset_list the usersets its access lists may name
 *
 * @return true if OK, else false
 *
 * @note MT-NOTE: ar_validate() is MT safe
 */
bool ar_validate(lListElem *ar, lList **alpp, bool in_master, bool is_spool, const lList *master_cqueue_list, 
                 const lList *master_hgroup_list, const lList *master_centry_list, const lList *master_ckpt_list,
                 const lList *master_pe_list, const lList *master_userset_list)
{
   uint64_t start_time;
   uint64_t end_time;
   uint64_t duration;
   uint64_t now64 = sge_get_gmt64();

   DENTER(TOP_LAYER);

   /*   AR_start_time, SGE_ULONG        */
   if ((start_time = lGetUlong64(ar, AR_start_time)) == 0) {
      start_time = now64;
      lSetUlong64(ar, AR_start_time, start_time);
   }

   /*   AR_end_time, SGE_ULONG        */
   end_time = lGetUlong64(ar, AR_end_time);
   duration = lGetUlong64(ar, AR_duration);
   
   if (end_time == 0 && duration == 0) {
      answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              MSG_AR_MISSING_VALUE_S, "end time or duration");
      goto ERROR;
   } else if (end_time == 0) {
      end_time = duration_add_offset(start_time, duration);
      duration = end_time  - start_time;
      lSetUlong64(ar, AR_end_time, end_time);
      lSetUlong64(ar, AR_duration, duration);
   } else if (duration == 0) {
      duration = end_time - start_time;
      lSetUlong64(ar, AR_duration, duration);
   }

   if ((end_time - start_time) != duration) {
      answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              MSG_AR_START_END_DURATION_INVALID);
      goto ERROR;
   }

   if (start_time > end_time) {
      answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              MSG_AR_START_LATER_THAN_END);
      goto ERROR;
   }
   
   if (!is_spool) {
      if (start_time < now64) {
         answer_list_add_sprintf(alpp, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                                 MSG_AR_START_IN_PAST);
         goto ERROR;
      }
   }
   /*   AR_owner, SGE_STRING */
   
   if (in_master) {
      /*    AR_name, SGE_STRING */
      NULL_OUT_NONE(ar, AR_name);
      if (object_verify_name(ar, alpp, AR_name)) {
         goto ERROR;
      }
      /*   AR_account, SGE_STRING */
      NULL_OUT_NONE(ar, AR_account);
      if (!lGetString(ar, AR_account)) {
         lSetString(ar, AR_account, DEFAULT_ACCOUNT);
      } else {
         if (verify_str_key(alpp, lGetString(ar, AR_account), MAX_VERIFY_STRING,
         "account string", QSUB_TABLE) != STATUS_OK) {
            goto ERROR;
         }
      }
      /*   AR_verify, SGE_ULONG              just verify the reservation or final case */
      /*   AR_error_handling, SGE_ULONG      how to deal with soft and hard exceptions */
      /*   AR_checkpoint_name, SGE_STRING    Named checkpoint */
      NULL_OUT_NONE(ar, AR_checkpoint_name);
      {
         /* request for non existing ckpt object will be refused */
         const char *ckpt_name = nullptr;

         ckpt_name = lGetString(ar, AR_checkpoint_name);
         if (ckpt_name != nullptr) {
            lListElem *ckpt_ep = ckpt_list_locate(master_ckpt_list, ckpt_name);
            if (!ckpt_ep) {
               ERROR(MSG_JOB_CKPTUNKNOWN_S, ckpt_name);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               goto ERROR;
            }
          }
      }
      /*   AR_resource_list, SGE_LIST */
      {
         if (centry_list_fill_request(lGetListRW(ar, AR_resource_list),
                                      alpp, master_centry_list, false, true, false)) {
            goto ERROR;
         }
         if (compress_ressources(alpp, lGetListRW(ar, AR_resource_list), SGE_OBJ_AR)) {
            goto ERROR;
         }
         
         if (!centry_list_is_correct(lGetListRW(ar, AR_resource_list), alpp)) {
            goto ERROR;
         }
      }
      /*   AR_queue_list, SGE_LIST */
      if (!qref_list_is_valid(lGetList(ar, AR_queue_list), alpp, master_cqueue_list, master_hgroup_list, master_centry_list)) {
         goto ERROR;
      }
      /*   AR_mail_options, SGE_ULONG   */
      /*   AR_mail_list, SGE_LIST */
      
      /*   AR_master_queue_list  -masterq wc_queue_list, SGE_LIST bind master task to queue(s) */
      if (!qref_list_is_valid(lGetList(ar, AR_master_queue_list), alpp, master_cqueue_list, master_hgroup_list, master_centry_list)) {
         goto ERROR;
      }
       
      
      /*   AR_pe, SGE_STRING,  AR_pe_range, SGE_LIST */
      NULL_OUT_NONE(ar, AR_pe);
      {
         const char *pe_name = nullptr;
         lList *pe_range = nullptr;
         
         pe_name = lGetString(ar, AR_pe);
         if (pe_name) {
            const lListElem *pep = pe_list_find_matching(master_pe_list, pe_name);
            if (!pep) {
               ERROR(MSG_JOB_PEUNKNOWN_S, pe_name);
               answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               goto ERROR;
            }
            /* check pe_range */
            pe_range = lGetListRW(ar, AR_pe_range);
            if (object_verify_pe_range(alpp, pe_name, pe_range, SGE_OBJ_AR)!=STATUS_OK) {
               goto ERROR;
            }
         }
      }

      /*   AR_acl_list, SGE_LIST */
      if (userset_list_validate_access(lGetList(ar, AR_acl_list), ARA_name, alpp, master_userset_list) != STATUS_OK) {
         goto ERROR;
      }
      
      /*   AR_xacl_list, SGE_LIST */
      if (userset_list_validate_access(lGetList(ar, AR_xacl_list), ARA_name, alpp, master_userset_list) != STATUS_OK) {
         goto ERROR;
      }

      if (is_spool) {
         dstring cqueue_buffer = DSTRING_INIT;
         dstring hostname_buffer = DSTRING_INIT;
         for_each_rw_lv(jg, lGetList(ar, AR_granted_slots)){
            const char *hostname = nullptr;
            const char *qname = lGetString(jg, JG_qname);
            bool has_hostname = false;
            bool has_domain = false;

            cqueue_name_split(qname, &cqueue_buffer, &hostname_buffer,
                              &has_hostname, &has_domain);
            hostname = sge_dstring_get_string(&hostname_buffer);
            lSetHost(jg, JG_qhostname, hostname);
         }
         sge_dstring_free(&cqueue_buffer);
         sge_dstring_free(&hostname_buffer);
      }
      /*   AR_type,  SGE_ULONG     */
      /*   AR_state, SGE_ULONG               state of the AR */
      if(lGetUlong(ar, AR_state) == ARL_UNKNOWN){
         lSetUlong(ar, AR_state, ARL_CREATION);  
      }
   }
   DRETURN(true);

ERROR:
   DRETURN(false);
}

/**
 * @brief Converts a string to a event id
 *
 * Converts a human readable event string to the corresponding
 * event if.
 *
 * @param string string
 *
 * @return the event id
 *
 * @note MT-NOTE: ar_get_event_from_string() is not MT safe
 */
ar_state_event_t
ar_get_event_from_string(const char *string)
{
   ar_state_event_t ret = ARL_UNKNOWN;

   DENTER(TOP_LAYER);
   if (string != nullptr) {
      if (!strcmp(MSG_AR_EVENT_STATE_UNKNOWN, string)) {
         ret = ARL_UNKNOWN;
      } else if (!strcmp(MSG_AR_EVENT_STATE_CREATION, string)) {
         ret = ARL_CREATION;
      } else if (!strcmp(MSG_AR_EVENT_STATE_STARTIME_REACHED, string)) {
         ret = ARL_STARTTIME_REACHED;
      } else if (!strcmp(MSG_AR_EVENT_STATE_ENDTIME_REACHED, string)) {
         ret = ARL_ENDTIME_REACHED;
      } else if (!strcmp(MSG_AR_EVENT_STATE_UNSATISFIED, string)) {
         ret = ARL_UNSATISFIED;
      } else if (!strcmp(MSG_AR_EVENT_STATE_OK, string)) {
         ret = ARL_OK;
      } else if (!strcmp(MSG_AR_EVENT_STATE_TERMINATED, string)) {
         ret = ARL_TERMINATED;
      } 
   } 
   DRETURN(ret);
}

/**
 * @brief Converts a state event to a string
 *
 * Converts a state event id to a human readable string.
 *
 * @param event state event id
 *
 * @return string
 *
 * @note MT-NOTE: ar_get_string_from_event() is not MT safe
 */
const char *
ar_get_string_from_event(ar_state_event_t event)
{
   const char *ret = MSG_AR_EVENT_STATE_UNKNOWN;
   DENTER(TOP_LAYER);
   switch(event) {
      case ARL_UNKNOWN:
         ret = MSG_AR_EVENT_STATE_UNKNOWN;
         break;
      case ARL_CREATION:
         ret = MSG_AR_EVENT_STATE_CREATION;
         break;
      case ARL_STARTTIME_REACHED:
         ret = MSG_AR_EVENT_STATE_STARTIME_REACHED;
         break;
      case ARL_ENDTIME_REACHED:
         ret = MSG_AR_EVENT_STATE_ENDTIME_REACHED;
         break;
      case ARL_UNSATISFIED:
         ret = MSG_AR_EVENT_STATE_UNSATISFIED;
         break;
      case ARL_OK:
         ret = MSG_AR_EVENT_STATE_OK;
         break;
      case ARL_TERMINATED:
         ret = MSG_AR_EVENT_STATE_TERMINATED;
         break;
      case ARL_DELETED:
         ret = MSG_AR_EVENT_STATE_DELETED;
         break;
      default:
         /* should never happen */
         DTRACE;
         break;
   }
   DRETURN(ret);
}

/**
 * @brief Writes the ar state as letter combination
 *
 * This function writes the given state of a advance reservation as
 * letter into the given dstring. The letter will be appended at the and.
 *
 * @param state ar state
 * @param state_as_string dstring
 *
 * @note MT-NOTE: ar_get_string_from_event() is MT safe
 */
void 
ar_state2dstring(ar_state_t state, dstring *state_as_string)
{
   const char *letter = "u";
   switch (state) {
      case AR_WAITING:
         letter = "w";
         break;
      case AR_RUNNING:
         letter = "r";
         break;
      case AR_EXITED:
         letter = "x";
         break;
      case AR_DELETED:
         letter = "d";
         break;
      case AR_ERROR:
         letter = "E";
         break;
      case AR_WARNING:
         letter = "W";
         break;
      default:
         break;
   }
   sge_dstring_append(state_as_string, letter);
}

/**
 * @brief Has AR errors?
 *
 * Check if one of the reserved queues is in state where jobs can not be
 * running
 *
 * @param ar advance reservation object (AR_Type)
 *
 * @return true if has errors false if has no errors
 *
 * @note MT-NOTE: sge_ar_has_errors() is MT safe
 */
bool sge_ar_has_errors(lListElem *ar) {
   bool ret = false;

   DENTER(TOP_LAYER);

   if (lGetUlong(ar, AR_qi_errors) != 0) {
      ret = true;
   }

   DRETURN(ret);
}
