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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cstring>
#include <sys/time.h>

#include "uti/sge_dstring.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_Session.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/ocs_DataStore.h"

#include "spool/sge_spooling.h"

#include "evm/sge_event_master.h"

#include "sge_c_gdi.h"
#include "sge_calendar_qmaster.h"
#include "sge_qinstance_qmaster.h"
#include "sge_utility_qmaster.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "msg_common.h"
#include "msg_qmaster.h"

void
calendar_initalize_timer(monitoring_t *monitor) {
   lListElem *cep;
   lList *ppList = nullptr;
   lList *answer_list = nullptr;
   const lList *master_calendar_list = *ocs::DataStore::get_master_list(SGE_TYPE_CALENDAR);

   DENTER(TOP_LAYER);

   for_each_rw (cep, master_calendar_list) {
      calendar_parse_year(cep, &answer_list);
      calendar_parse_week(cep, &answer_list);
      answer_list_output(&answer_list);

      ocs::gdi::Packet packet;
      ocs::gdi::Task task{};
      packet.gdi_session = ocs::SessionManager::GDI_SESSION_NONE;
      calendar_update_queue_states(&packet, &task, cep, nullptr, nullptr, &ppList, monitor);
   }

   lFreeList(&answer_list);
   lFreeList(&ppList);

   DRETURN_VOID;
}

int
calendar_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *new_cal, lListElem *cep, int add,
             const char *ruser, const char *rhost, gdi_object_t *object,
             ocs::gdi::Command::Cmd cmd, ocs::gdi::SubCommand::SubCmd sub_command, monitoring_t *monitor) {
   const lList *master_ar_list = *ocs::DataStore::get_master_list(SGE_TYPE_AR);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lListElem *cqueue;
   const char *cal_name;

   DENTER(TOP_LAYER);

   /* ---- CAL_name cannot get changed - we just ignore it */
   if (add == 1) {
      cal_name = lGetString(cep, CAL_name);
      if (verify_str_key(alpp, cal_name, MAX_VERIFY_STRING, "calendar", KEY_TABLE) != STATUS_OK)
         goto ERROR;
      lSetString(new_cal, CAL_name, cal_name);
   } else {
      cal_name = lGetString(new_cal, CAL_name);
   }

   /* ---- CAL_year_calendar */
   attr_mod_zerostr(cep, new_cal, CAL_year_calendar, "year calendar");
   if (lGetPosViaElem(cep, CAL_year_calendar, SGE_NO_ABORT) >= 0) {
      if (!calendar_parse_year(new_cal, alpp))
         goto ERROR;
   }

   /* ---- CAL_week_calendar */
   attr_mod_zerostr(cep, new_cal, CAL_week_calendar, "week calendar");
   if (lGetPosViaElem(cep, CAL_week_calendar, SGE_NO_ABORT) >= 0) {
      if (!calendar_parse_week(new_cal, alpp))
         goto ERROR;
   }

   if (add != 1) {
      for_each_ep(cqueue, master_cqueue_list) {
         const lListElem *queue;
         for_each_ep(queue, lGetList(cqueue, CQ_qinstances)) {
            const char *q_cal = lGetString(queue, QU_calendar);
            if ((q_cal != nullptr) && (strcmp(cal_name, q_cal) == 0)) {
               if (sge_ar_list_conflicts_with_calendar(alpp, lGetString(queue, QU_full_name), new_cal, master_ar_list)) {
                  goto ERROR;
               }
            }
         }
      }
   }

   DRETURN(0);

   ERROR:
DRETURN(STATUS_EUNKNOWN);
}

int
calendar_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *cep, gdi_object_t *object) {
   lList *answer_list = nullptr;

   DENTER(TOP_LAYER);

   bool dbret = spool_write_object(&answer_list, spool_get_default_context(), cep,
                                   lGetString(cep, CAL_name), SGE_TYPE_CALENDAR, true);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, lGetString(cep, CAL_name));
   }

   DRETURN(dbret ? 0 : 1);
}

int
sge_del_calendar(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *cep, lList **alpp, char *ruser, char *rhost) {
   const char *cal_name;
   lList **master_calendar_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_CALENDAR);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   DENTER(TOP_LAYER);

   if (!cep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ep is no calendar element, if cep has no CAL_name */
   if (lGetPosViaElem(cep, CAL_name, SGE_NO_ABORT) < 0) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(QU_qname), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }
   cal_name = lGetString(cep, CAL_name);

   if (!lGetElemStrRW(*master_calendar_list, CAL_name, cal_name)) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_CALENDAR, cal_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* prevent deletion of a still referenced calendar */
   {
      lList *local_answer_list = nullptr;

      if (calendar_is_referenced(cep, &local_answer_list, master_cqueue_list)) {
         const lListElem *answer = lFirst(local_answer_list);

         ERROR("denied: %s", lGetString(answer, AN_text));
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC,
                         ANSWER_QUALITY_ERROR);
         lFreeList(&local_answer_list);
         DRETURN(STATUS_ESEMANTIC);
      }
   }

   /* remove timer for this calendar */
   te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, cal_name);

   sge_event_spool(alpp, 0, sgeE_CALENDAR_DEL, 0, 0, cal_name, nullptr, nullptr,
                   nullptr, nullptr, nullptr, true, true,packet->gdi_session);
   lDelElemStr(master_calendar_list, CAL_name, cal_name);

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, cal_name, MSG_OBJ_CALENDAR);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(STATUS_OK);
}

/****** qmaster/sge_calendar_qmaster/sge_calendar_event_handler() **************
*  NAME
*     sge_calendar_event_handler() -- calendar event handler
*
*  SYNOPSIS
*     void sge_calendar_event_handler(te_event_t anEvent) 
*
*  FUNCTION
*     Handle calendar events. 
*
*  INPUTS
*     te_event_t anEvent - calendar event
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_calendar_event_handler() is MT safe 
*
*******************************************************************************/
void sge_calendar_event_handler(te_event_t anEvent, monitoring_t *monitor) {
   lListElem *cep;
   const char *cal_name = te_get_alphanumeric_key(anEvent);
   lList *ppList = nullptr;
   const lList *master_calendar_list = *ocs::DataStore::get_master_list(SGE_TYPE_CALENDAR);

   DENTER(TOP_LAYER);

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   if (!(cep = lGetElemStrRW(master_calendar_list, CAL_name, cal_name))) {
      ERROR(MSG_EVE_TE4CAL_S, cal_name);
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
      DRETURN_VOID;
   }

   {
      // the calendar timer is hard to diagnose after the fact - record that the event which was
      // announced by calendar_update_queue_states() has indeed been delivered, and when
      DSTRING_STATIC(due_dstr, 64);
      DSTRING_STATIC(now_dstr, 64);
      INFO(MSG_CALENDAR_TIMEREVENT_SSS, cal_name, sge_ctime64(te_get_when(anEvent), &due_dstr),
           sge_ctime64(sge_get_gmt64(), &now_dstr));
   }

   ocs::gdi::Packet packet;
   ocs::gdi::Task task;
   packet.gdi_session = ocs::SessionManager::GDI_SESSION_NONE;
   calendar_update_queue_states(&packet, &task, cep, nullptr, nullptr, &ppList, monitor);

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   lFreeList(&ppList);
   sge_free(&cal_name);

   DRETURN_VOID;
} /* sge_calendar_event_handler() */

/****** qmaster/sge_calendar_qmaster/calendar_arm_timer() **********************
*  NAME
*     calendar_arm_timer() -- schedule the next state change of a calendar
*
*  FUNCTION
*     Adds a one time event that makes the calendar be re-evaluated when its next
*     state change is due. Any event that is still pending for the calendar is
*     removed first: delivering an event recomputes the state of every queue
*     instance using the calendar, so duplicates would repeat that sweep once per
*     event, and the callers cannot tell whether a timer has been armed already.
*
*  INPUTS
*     const char *cal_name - name of the calendar
*     u_long64 when        - time of the next state change, must not be 0
*
*  NOTES
*     MT-NOTE: calendar_arm_timer() is MT safe
*******************************************************************************/
void
calendar_arm_timer(const char *cal_name, u_long64 when) {
   DENTER(TOP_LAYER);

   te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, cal_name);

   te_event_t ev = te_new_event(when, TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, cal_name);
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
}

int calendar_update_queue_states(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *cep, lListElem *old_cep, gdi_object_t *object,
                                 lList **ppList, monitoring_t *monitor) {
   const char *cal_name = lGetString(cep, CAL_name);
   lList *state_changes_list = nullptr;
   u_long32 state;
   u_long64 when = 0;
   DENTER(TOP_LAYER);

   sge_add_event(0, old_cep != nullptr ? sgeE_CALENDAR_MOD : sgeE_CALENDAR_ADD, 0, 0, cal_name, nullptr, nullptr, cep, packet->gdi_session);

   state = calender_state_changes(cep, &state_changes_list, &when, nullptr);

   qinstance_change_state_on_calendar_all(cal_name, state, state_changes_list, monitor, packet->gdi_session);

   lFreeList(&state_changes_list);

   if (when != 0) {
      DSTRING_STATIC(when_dstr, 64);
      INFO(MSG_CALENDAR_NEXTSTATECHANGE_SUS, cal_name, state, sge_ctime64(when, &when_dstr));

      calendar_arm_timer(cal_name, when);
   } else {
      // no timer is armed - from now on only a calendar or queue modification can change the state
      INFO(MSG_CALENDAR_NOSTATECHANGE_SU, cal_name, state);
   }

   DRETURN(0);
}

