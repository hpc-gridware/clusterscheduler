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

/** @file
 * @brief Calendars, and the queue states they drive
 */
#include <cstdio>
#include <cstring>
#include <sys/time.h>

#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

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

/** @brief Arm the timers for every calendar known at startup
 *
 * @param monitor for monitoring qmaster threads
 */
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

/** @brief Apply one attribute change to a calendar
 *
 * The gdi_object_t::modifier for calendars; see sge_c_gdi.h for the sequence it is called from.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param new_cal see the declaration
 * @param cep see the declaration
 * @param add 1 for add, 0 for modify
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @param object the table entry for this object type
 * @param cmd the command being executed
 * @param sub_command what kind of modification this is
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
calendar_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *new_cal, lListElem *cep, int add,
             const char *ruser, const char *rhost, gdi_object_t *object,
             ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   const lList *master_ar_list = *ocs::DataStore::get_master_list(SGE_TYPE_AR);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const char *cal_name;

   DENTER(TOP_LAYER);

   /* ---- CAL_name cannot get changed - we just ignore it */
   if (add == 1) {
      cal_name = lGetString(cep, CAL_name);
      if (verify_obj_name(alpp, cal_name, MAX_VERIFY_STRING, "calendar") != STATUS_OK)
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
      for_each_ep_lv(cqueue, master_cqueue_list) {
         for_each_ep_lv(queue, lGetList(cqueue, CQ_qinstances)) {
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

/** @brief Write a calendar to the spool
 *
 * The gdi_object_t::writer for calendars.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param cep see the declaration
 * @param object the table entry for this object type
 * @return 0 on success
 */
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

/** @brief Delete a calendar, unless a queue still follows it
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cep the calendar
 * @param alpp receives messages for the caller
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @return STATUS_OK on success
 */
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

/**
 * @brief Calendar event handler
 *
 * Handle calendar events.
 *
 * @param anEvent calendar event
 * @param monitor for monitoring qmaster threads
 *
 * @note MT-NOTE: sge_calendar_event_handler() is MT safe
 */
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

   ocs::gdi::Packet packet;
   ocs::gdi::Task task;
   packet.gdi_session = ocs::SessionManager::GDI_SESSION_NONE;
   calendar_update_queue_states(&packet, &task, cep, nullptr, nullptr, &ppList, monitor);

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   lFreeList(&ppList);
   sge_free(&cal_name);

   DRETURN_VOID;
} /* sge_calendar_event_handler() */

/** @brief Put the queues following a calendar into the state it now prescribes
 *
 * Runs both when the calendar is changed and when a calendar event fires, so a
 * queue reaches the right state whether the change came from an administrator
 * or from the clock.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cep the calendar
 * @param old_cep the calendar as it was, or nullptr when a timer triggered this
 * @param object the table entry for calendars, or nullptr when a timer triggered this
 * @param ppList receives information for post processing
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int calendar_update_queue_states(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *cep, lListElem *old_cep, gdi_object_t *object,
                                 lList **ppList, monitoring_t *monitor) {
   const char *cal_name = lGetString(cep, CAL_name);
   lList *state_changes_list = nullptr;
   uint32_t state;
   uint64_t when = 0;
   DENTER(TOP_LAYER);

   sge_add_event(0, old_cep != nullptr ? sgeE_CALENDAR_MOD : sgeE_CALENDAR_ADD, 0, 0, cal_name, nullptr, nullptr, cep, packet->gdi_session);

   state = calender_state_changes(cep, &state_changes_list, &when, nullptr);

   qinstance_change_state_on_calendar_all(cal_name, state, state_changes_list, monitor, packet->gdi_session);

   lFreeList(&state_changes_list);

   if (when != 0) {
      te_event_t ev;

      ev = te_new_event(when, TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, cal_name);
      te_add_event(ev);
      te_free_event(&ev);
   }

   DRETURN(0);
}

