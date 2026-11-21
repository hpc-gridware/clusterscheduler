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

#include "uti/sge_dstring.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_Session.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_AN.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_CQU.h"
#include "tom/ocs_List.h"

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
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   const auto master_calendar_list = ocs::DataStore::master_list_rw<SGE_TYPE_CALENDAR>();

   for (const auto cep : master_calendar_list) {
      calendar_parse_year(cep, &answer_list);
      calendar_parse_week(cep, &answer_list);
      answer_list_output(&answer_list);

      ocs::gdi::Packet packet;
      ocs::gdi::Task task{};
      packet.gdi_session = ocs::SessionManager::GDI_SESSION_NONE;
      calendar_update_queue_states(&packet, &task, cep, nullptr, monitor);
   }

   answer_list.reset();

   DRETURN_VOID;
}

/** @brief Apply one attribute change to a calendar
 *
 * The modifier of calendars, bound in ocs_gdi_hooks<ocs::tom::CAL>; see ocs_gdi_add_mod() in
 * sge_c_gdi.cc for the sequence it is called from.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param new_cal the calendar being built
 * @param cep the calendar as sent by the client, may be reduced
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
calendar_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
             const ocs::tom::Element<ocs::tom::CAL> new_cal, const ocs::tom::ConstElement<ocs::tom::CAL> cep,
             int add, const char *ruser, const char *rhost, gdi_object_t *object,
             ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const auto master_ar_list = ocs::DataStore::master_list<SGE_TYPE_AR>();
   const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
   const char *cal_name;

   /* ---- CAL_name cannot get changed - we just ignore it */
   if (add == 1) {
      cal_name = cep.get<CAL_name>();
      if (verify_obj_name(alpp, cal_name, MAX_VERIFY_STRING, "calendar") != STATUS_OK)
         goto ERROR;
      new_cal.set<CAL_name>(cal_name);
   } else {
      cal_name = new_cal.get<CAL_name>();
   }

   /* ---- CAL_year_calendar */
   new_cal.copy_if_present<CAL_year_calendar>(cep);
   if (cep.has<CAL_year_calendar>()) {
      if (!calendar_parse_year(new_cal, alpp))
         goto ERROR;
   }

   /* ---- CAL_week_calendar */
   new_cal.copy_if_present<CAL_week_calendar>(cep);
   if (cep.has<CAL_week_calendar>()) {
      if (!calendar_parse_week(new_cal, alpp))
         goto ERROR;
   }

   if (add != 1) {
      for (const auto cqueue : master_cqueue_list) {
         for (ocs::tom::ConstElement<ocs::tom::QU> queue : cqueue.get<CQ_qinstances>()) {
            const char *q_cal = queue.get<QU_calendar>();
            if ((q_cal != nullptr) && (strcmp(cal_name, q_cal) == 0)) {
               if (sge_ar_list_conflicts_with_calendar(alpp, queue.get<QU_full_name>(), new_cal,
                                                       master_ar_list)) {
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
 * The writer of calendars, bound in ocs_gdi_hooks<ocs::tom::CAL> (sge_c_gdi.cc).
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param cep the calendar
 * @param object the table entry for this object type
 * @return 0 on success
 */
int
calendar_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
               const ocs::tom::ConstElement<ocs::tom::CAL> cep, gdi_object_t *object) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

   bool dbret = spool_write_object(&answer_list, spool_get_default_context(), cep,
                                   cep.get<CAL_name>(), SGE_TYPE_CALENDAR);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, cep.get<CAL_name>());
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
sge_del_calendar(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::CAL> cep,
                 ocs::tom::ListPtr<ocs::tom::AN> *alpp, char *ruser, char *rhost) {
   DENTER(TOP_LAYER);

   const char *cal_name;
   const auto master_calendar_list = ocs::DataStore::master_list_rw<SGE_TYPE_CALENDAR>();
   const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();

   if (!cep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ep is no calendar element, if cep has no CAL_name */
   if (!cep.has<CAL_name>()) {
      CRITICAL(MSG_SGETEXT_MISSINGFIELD_SS, ocs::tom::FieldTraits<CAL_name>::info.name, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }
   cal_name = cep.get<CAL_name>();

   if (!master_calendar_list.find_first<CAL_name>(cal_name)) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_CALENDAR, cal_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* prevent deletion of a still referenced calendar */
   {
      ocs::tom::ListPtr<ocs::tom::AN> local_answer_list;

      if (calendar_is_referenced(cep, &local_answer_list, master_cqueue_list)) {
         // the answer list API stays raw by design
         const ocs::tom::ConstElement<ocs::tom::AN> answer =
                 ocs::tom::ConstList<ocs::tom::AN>(local_answer_list).front();

         ERROR("denied: %s", answer.get<AN_text>());
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC,
                         ANSWER_QUALITY_ERROR);
         local_answer_list.reset();
         DRETURN(STATUS_ESEMANTIC);
      }
   }

   /* remove timer for this calendar */
   te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, cal_name);

   sge_event_spool(alpp, 0, sgeE_CALENDAR_DEL, 0, 0, cal_name, nullptr, nullptr,
                   nullptr, nullptr, nullptr, true, true,packet->gdi_session);
   master_calendar_list.erase<CAL_name>(cal_name);

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
   DENTER(TOP_LAYER);

   const char *cal_name = te_get_alphanumeric_key(anEvent);

   MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);

   const auto master_calendar_list = ocs::DataStore::master_list_rw<SGE_TYPE_CALENDAR>();

   const ocs::tom::Element<ocs::tom::CAL> cep = master_calendar_list.find_first<CAL_name>(cal_name);
   if (!cep) {
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
   calendar_update_queue_states(&packet, &task, cep, nullptr, monitor);

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);

   sge_free(&cal_name);

   DRETURN_VOID;
} /* sge_calendar_event_handler() */

/** @brief Schedule the next state change of a calendar
 *
 * Adds a one time event that makes the calendar be re-evaluated when its next
 * state change is due. Any event that is still pending for the calendar is
 * removed first: delivering an event recomputes the state of every queue
 * instance using the calendar, so duplicates would repeat that sweep once per
 * event, and the callers cannot tell whether a timer has been armed already.
 *
 * @param cal_name name of the calendar
 * @param when time of the next state change, must not be 0
 *
 * @note MT-NOTE: calendar_arm_timer() is MT safe
 */
void
calendar_arm_timer(const char *cal_name, uint64_t when) {
   DENTER(TOP_LAYER);

   te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, cal_name);

   te_event_t ev = te_new_event(when, TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, cal_name);
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
}

/** @brief Put the queues following a calendar into the state it now prescribes
 *
 * Runs both when the calendar is changed and when a calendar event fires, so a
 * queue reaches the right state whether the change came from an administrator
 * or from the clock.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cep the calendar
 * @param old_cep the calendar as it was, or an empty handle when a timer triggered this
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
calendar_update_queue_states(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::CAL> cep,
                             const ocs::tom::ConstElement<ocs::tom::CAL> old_cep, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *cal_name = cep.get<CAL_name>();
   sge_add_event(0, old_cep != nullptr ? sgeE_CALENDAR_MOD : sgeE_CALENDAR_ADD, 0, 0, cal_name, nullptr, nullptr, cep,
                 packet->gdi_session);

   auto [state, when, state_changes_list] = calender_state_changes(cep, nullptr);

   qinstance_change_state_on_calendar_all(cal_name, state, state_changes_list.get(), monitor, packet->gdi_session);

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

/** @brief calendar_update_queue_states() as the on_success of calendars, bound in ocs_gdi_hooks<ocs::tom::CAL>
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cep the calendar
 * @param old_cep the calendar as it was, or an empty handle when it was added
 * @param object the table entry for calendars, unused
 * @param post what is left for after the last object of the request, unused
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
calendar_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::CAL> cep,
                 const ocs::tom::ConstElement<ocs::tom::CAL> old_cep, [[maybe_unused]] gdi_object_t *object,
                 [[maybe_unused]] ocs_gdi_post_processing *post, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   const int ret = calendar_update_queue_states(packet, task, cep, old_cep, monitor);
   DRETURN(ret);
}
