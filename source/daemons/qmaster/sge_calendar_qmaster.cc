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

#include "spool/sge_spooling.h"
#include "spool/flatfile/sge_flatfile.h"
#include "spool/flatfile/sge_flatfile_obj.h"

#include "procedure/qconf/ocs_QConfObject.h"

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

   lListElem *cep;
   lList *ppList = nullptr;
   lList *answer_list = nullptr;
   const lList *master_calendar_list = *ocs::DataStore::get_master_list(SGE_TYPE_CALENDAR);

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

/** @brief Modify one calendar on behalf of a stored procedure
 *
 * Installed as the provider the procedure library calls (CS-2822). It exists
 * here and not in `libs/procedure` because `sge_gdi_add_mod_generic()` is the
 * daemon's and the procedure library is linked into the clients as well.
 *
 * Two halves, and both are deliberately the ones that already exist:
 *
 *  - the text is parsed by `spool_flatfile_read_object()` with the very
 *    descriptor and field list a client uses for this object type, through the
 *    string source the reader gained for this purpose. Parsing in qmaster is
 *    what makes a reduced read editable at all: only qmaster knows which values
 *    it masked, so only qmaster can tell a placeholder from a new value.
 *  - the parsed element then goes through `sge_gdi_add_mod_generic()`, the same
 *    function a `qconf -mcal` reaches, so `calendar_mod()` validates it,
 *    `calendar_spool()` spools it and `calendar_update_queue_states()` does the
 *    follow-up work and emits the event. Nothing about the write is new.
 *
 * @param packet the client request, for the user and host the change is logged under
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @param parameter the object text to apply
 * @param answer_list receives the messages parsing and writing produced
 * @return true when the calendar was modified
 */
bool
calendar_mod_from_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                            const ocs::CalendarModParameter &parameter, lList **answer_list) {
   DENTER(TOP_LAYER);

   gdi_object_t *object = get_gdi_object(ocs::gdi::Target::CAL_LIST);
   if (object == nullptr) {
      answer_list_add(answer_list, MSG_UNKNOWN_OBJECT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   /* The text is in the form the read rendered it, which is what the caller's
    * -fmt asked for - so the same value decides how it is parsed back. */
   spool_flatfile_format format = SP_FORM_ASCII;
   switch (parameter.get_output_format()) {
      case ocs::ProcedureParameter::OutputFormat::XML:
         format = SP_FORM_XML;
         break;
      case ocs::ProcedureParameter::OutputFormat::JSON:
         format = SP_FORM_JSON;
         break;
      case ocs::ProcedureParameter::OutputFormat::PLAIN:
      default:
         format = SP_FORM_ASCII;
         break;
   }

   const qconf_object_desc &desc = qconf_desc(QConfObj::CAL);
   bool fields_owned = false;
   spooling_field *fields = desc.build_fields(format, &fields_owned);
   int fields_out[MAX_NUM_FIELDS];
   fields_out[0] = NoName;

   lListElem *instructions = spool_flatfile_read_object(answer_list, desc.descr, nullptr, fields,
                                                        fields_out, true, desc.sfi, format,
                                                        nullptr, nullptr,
                                                        parameter.get_object_text().c_str());
   if (instructions == nullptr) {
      // the reader wrote the reason
      if (fields_owned) {
         spool_free_spooling_fields(fields);
      }
      DRETURN(false);
   }

   /* Checked before the fields are freed, as the client does it: a name the
    * reader did not consume means the text carries something this object type
    * has no attribute for, and applying the rest of it would silently drop it. */
   const int unprocessed = spool_get_unprocessed_field(fields, fields_out, answer_list);
   if (fields_owned) {
      spool_free_spooling_fields(fields);
   }
   if (unprocessed != NoName) {
      lFreeElem(&instructions);
      DRETURN(false);
   }

   /* add == 0: this modifies an existing calendar and must fail when there is
    * none, which sge_gdi_add_mod_generic() enforces before the modifier runs.
    * Which calendar is decided by the name in the text, exactly as it is for
    * "qconf -Mcal file" today. */
   const int result = sge_gdi_add_mod_generic(packet, task, answer_list, instructions, 0, object,
                                              packet->user, packet->host,
                                              ocs::gdi::Command::MOD_PROCEDURE,
                                              ocs::gdi::SubCommand::NONE, nullptr, monitor);
   lFreeElem(&instructions);

   DRETURN(result == STATUS_OK);
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
   DENTER(TOP_LAYER);

   const lList *master_ar_list = *ocs::DataStore::get_master_list(SGE_TYPE_AR);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const char *cal_name;

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
   DENTER(TOP_LAYER);

   lList *answer_list = nullptr;

   bool dbret = spool_write_object(&answer_list, spool_get_default_context(), cep,
                                   lGetString(cep, CAL_name), SGE_TYPE_CALENDAR);
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
   DENTER(TOP_LAYER);

   const char *cal_name;
   lList **master_calendar_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_CALENDAR);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

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
   DENTER(TOP_LAYER);

   lListElem *cep;
   const char *cal_name = te_get_alphanumeric_key(anEvent);
   lList *ppList = nullptr;
   const lList *master_calendar_list = *ocs::DataStore::get_master_list(SGE_TYPE_CALENDAR);

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
*     uint64_t when        - time of the next state change, must not be 0
*
*  NOTES
*     MT-NOTE: calendar_arm_timer() is MT safe
*******************************************************************************/
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
 * @param old_cep the calendar as it was, or nullptr when a timer triggered this
 * @param object the table entry for calendars, or nullptr when a timer triggered this
 * @param ppList receives information for post processing
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int calendar_update_queue_states(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *cep, lListElem *old_cep, gdi_object_t *object,
                                 lList **ppList, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *cal_name = lGetString(cep, CAL_name);
   lList *state_changes_list = nullptr;
   uint32_t state;
   uint64_t when = 0;
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

