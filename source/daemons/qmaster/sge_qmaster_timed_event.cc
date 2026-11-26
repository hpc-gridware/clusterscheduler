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
 *  Copyright: 2003 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The timer qmaster hangs all its periodic and deferred work on
 */

#include <cstring>
#include <cerrno>
#include <pthread.h>

#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_thread_ctrl.h"
#include "uti/ocs_cond.h"

#include "tom/ocs_Types.h"

#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Select.h"
#include "tom/ocs_SortCore.h"
#include "sgeobj/tom/ocs_tom_TE.h"

#include "sge_qmaster_timed_event.h"

#include "msg_common.h"
#include "msg_qmaster.h"

/** @brief Debug layer the timed event code logs under */
#define EVENT_LAYER TOM_LAYER

event_control_t Event_Control = {
        PTHREAD_MUTEX_INITIALIZER,
        PTHREAD_COND_INITIALIZER,
        false,
        false,
        nullptr,
        nullptr,
        0,
        0,
        0
};

static int event_control_cond_init_ret = ocs::uti::condition_initialize(&Event_Control.cond_var);

handler_tbl_t Handler_Tbl = {
        PTHREAD_MUTEX_INITIALIZER,
        *new std::vector<tbl_elem>
};

/**
 * @brief The alphanumeric key of an event for a log or debug message
 *
 * @param anEvent timed event
 * @return the key, or MSG_SMALLNULL if the event has none
 */
static const char *te_str_key_for_message(const te_event *anEvent) {
   return anEvent->str_key ? anEvent->str_key->c_str() : MSG_SMALLNULL;
}

/**
 * @brief Delete one time events
 *
 * Delete one or more one-time events. All one-time events which do EXACTLY
 * match the given arguments will be deleted from the event list if
 * ignore_keys is false. Otherwise all one-time events of the given type
 * will be removed.
 * If a timed event is scheduled for delivery, it will NOT be deleted,
 * even if it does match the arguments. Such an event will be deleted after
 * event delivery has been finished.
 *
 * @param aType event type
 * @param aKey1 first numeric key
 * @param aKey2 second numeric key
 * @param strKey alphanumeric key
 * @param ignore_keys boolean flag
 *
 * @return number of events deleted
 *
 * @note MT-NOTE: te_delete_all_or_one_time_event() is MT safe.
 *       MT-NOTE:
 *       MT-NOTE: If a timed event has been deleted we need to signal the event
 *       MT-NOTE: delivery thread. This is because the event delivery thread
 *       MT-NOTE: maybe waiting until the just deleted event becomes due. Event
 *       MT-NOTE: deletion is communicated by setting 'Event_Control.deleted'
 *       MT-NOTE: to 'true'.
 */
static int
te_delete_all_or_one_time_event(te_type_t aType, uint32_t aKey1, uint32_t aKey2, const char *strKey, bool ignore_keys) {
   DENTER(EVENT_LAYER);

   int res = 0;

   DPRINTF("%s: (t:" sge_u32" u1:" sge_u32" u2:" sge_u32" s:%s)\n", __func__,
           static_cast<uint32_t>(aType), aKey1, aKey2, strKey ? strKey : MSG_SMALLNULL);

   // the events to keep: every event that differs from the one(s) to delete
   auto keep = ocs::tom::field<TE_type>() != static_cast<uint32_t>(aType) ||
               ocs::tom::field<TE_mode>() != static_cast<uint32_t>(ONE_TIME_EVENT);
   if (!ignore_keys) {
      keep = std::move(keep) || ocs::tom::field<TE_uval0>() != aKey1 || ocs::tom::field<TE_uval1>() != aKey2;
      if (strKey != nullptr) {
         keep = std::move(keep) || ocs::tom::field<TE_sval>() != strKey;
      }
   }

   sge_mutex_lock("event_control_mutex", __func__, __LINE__, &Event_Control.mutex);

   // Event_Control.list is a process-lifetime global created as TE in te_init()
   const auto event_list = ocs::tom::List<ocs::tom::TE>::from_typed(Event_Control.list);
   if (event_list.empty()) {
      DPRINTF("%s: event list empty!\n", __func__);

      sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &Event_Control.mutex);

      DRETURN(0);
   }

   // unlike the former lSplit(), remove_if() keeps the emptied list, so it is never recreated here
   res = static_cast<int>(event_list.remove_if(!std::move(keep)));

   if (res > 0) {
      Event_Control.deleted = true;

      pthread_cond_signal(&Event_Control.cond_var);

      DPRINTF("%s: did delete %d event!\n", __func__, res);
   }

   sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &Event_Control.mutex);

   DRETURN(res);
}

/**
 * @brief Waits, if the event list is empty
 *
 * waits, if the event list is empty
 *
 * @note MT-NOTE: te_wait_empty() is not MT safe
 */
void te_wait_empty() {
   DENTER(EVENT_LAYER);

   while (ocs::tom::ConstList<ocs::tom::TE>::from_typed(Event_Control.list).empty()) {
      DPRINTF("%s: event list empty --> will wait\n", __func__);
      Event_Control.next = 0;
      pthread_cond_wait(&Event_Control.cond_var, &Event_Control.mutex);
   }

   DRETURN_VOID;
}

/**
 * @brief Waits for the next event
 *
 * waits for the next event
 *
 * @param te next event
 * @param now current time
 *
 * @note MT-NOTE: te_wait_next() is not MT safe
 */
void te_wait_next(te_event_t te, uint64_t now) {
   DENTER(EVENT_LAYER);

#if 0
   INFO("==> te_wait_next()");
   DSTRING_STATIC(dstr, 100);
   INFO("   -> now:  %s", sge_ctime64(now, &dstr));
   INFO("   -> next: %s", sge_ctime64(Event_Control.next, &dstr));
   INFO("   -> when: %s", sge_ctime64(te->when, &dstr));
   INFO("   -> diff: %f", sge_gmt64_to_gmt32_double(diff));
#endif

   while (Event_Control.next == te->when && te->when > now) {
      uint64_t diff = te->when - now;
      int res = ocs::uti::condition_timedwait(&Event_Control.cond_var, &Event_Control.mutex, diff / 1000000, diff % 1000000);
      if (res == ETIMEDOUT) {
         // the events (te) execution time is due, return to the caller where the timed event will be executed
         break;
      } else {
         // condition variable was signalled - there are two possible reasons:
         // 1. the event list has been modified
         //     - if a modification was done behind the current event, continue waiting (done by the while above)
         //     - if a modification was done before the current event, e.g. a new event has been added which shall
         //       be executed earlier than the current event, the while above will break the loop as then next < when
         // 2. when shutting down sge_qmaster we signal the condition to wake up the thread
         if (sge_thread_has_shutdown_started()) {
            break;
         }

         // update now, it is termination condition in the while above
         now = sge_get_gmt64();
      }
   }

#if 0
   INFO("==> te_wait_next() done");
#endif

   DRETURN_VOID;
}

/**
 * @brief Register event handler
 *
 * Register an event handler. The registered handler will be invoked whenever
 * an event of type 'aType' is due.
 * If more than one event handler for a given event type will be registered,
 * only the FIRST event handler registered will be invoked. It is possible,
 * however, to register the same event handler for multiple event types.
 *
 * @param aHandler event handler
 * @param aType event type
 *
 * @note MT-NOTE: te_register_event_handler() is MT safe.
 */
void te_register_event_handler(te_handler_t aHandler, te_type_t aType) {
   DENTER(EVENT_LAYER);

   SGE_ASSERT(aHandler != nullptr);

   sge_mutex_lock("handler_table_mutex", __func__, __LINE__, &Handler_Tbl.mutex);

   Handler_Tbl.list.push_back({aType, aHandler});

   DPRINTF("%s: handler #%zu for event type %d\n", __func__, Handler_Tbl.list.size() - 1, aType);

   sge_mutex_unlock("handler_table_mutex", __func__, __LINE__, &Handler_Tbl.mutex);

   DRETURN_VOID;
} /* te_register_event_handler() */

/**
 * @brief Allocate new timed event
 *
 * Allocate and initialize a new timed event. The new event will be
 * initialized using the arguments given.
 * The caller of this function is responsible to free the timed event
 * returned, using 'te_free_event()'.
 * If event type is 'ONE_TIME_EVENT', 'aTime' does determine the ABSOLUTE
 * timed event due time in seconds since the Epoch. If event type is
 * 'RECURRING_EVENT', 'aTime' does determine the timed event INTERVAL in
 * seconds.
 * If 'aStrKey' is not 'nullptr', the new timed event will contain a copy.
 *
 * @param aTime event due time or interval
 * @param aType event type
 * @param aMode event mode
 * @param aKey1 first numeric key, '0' if not used
 * @param aKey2 second numeric key, '0' if not used
 * @param aStrKey alphanumeric key, 'nullptr' if not used
 *
 * @return new timed event
 *
 * @note MT-NOTE: te_new_event() is MT safe.
 */
te_event_t
te_new_event(uint64_t aTime, te_type_t aType, te_mode_t aMode, uint32_t aKey1, uint32_t aKey2, const char *aStrKey) {
   DENTER(EVENT_LAYER);

   te_event_t ev = ocs::uti::create<te_event>();

   if (ONE_TIME_EVENT == aMode) {
      ev->when = aTime;
      ev->interval = 0;
   } else {
      ev->when = 0;
      ev->interval = aTime;
   }

   ev->type = aType;
   ev->mode = aMode;
   ev->ulong_key_1 = aKey1;
   ev->ulong_key_2 = aKey2;
   if (aStrKey != nullptr) {
      ev->str_key = aStrKey;
   }
   ev->seq_no = 0;

   DRETURN(ev);
} /* te_new_event() */

/**
 * @brief Free timed event
 *
 * Destroys the event made by te_new_event() or te_event_from_list_elem(). Upon return, '*anEvent' is 'nullptr',
 * and a key returned by te_get_alphanumeric_key() for it is no longer valid.
 *
 * @param anEvent pointer to the timed event; the pointer itself must NOT be 'nullptr'
 *
 * @note MT-NOTE: te_free_event() is MT safe.
 */
void
te_free_event(te_event_t *anEvent) {
   DENTER(EVENT_LAYER);

   SGE_ASSERT((anEvent != nullptr));

   ocs::uti::destroy(*anEvent);

   DRETURN_VOID;
} /* te_free_event() */

/**
 * @brief Add timed event
 *
 * Add timed event. An event handler which does match the event type of
 * 'anEvent' must have been registered previously. Otherwise the timed
 * event 'anEvent' will not be delivered.
 * After event delivery an event with event mode 'ONE_TIME_EVENT' will be
 * removed. An event with event mode 'RECURRING_EVENT' will be delivered
 * repeatedly until being removed explicitly, using 'te_delete_event()'.
 * The event 'anEvent' could be freed safely, using 'te_free_event()' after
 * this function did return.
 *
 * @param anEvent timed event
 *
 * @note MT-NOTE: te_add_event() is MT safe.
 *       MT-NOTE:
 *       MT-NOTE: If the event list is empty, the event delivery thread will wait
 *       MT-NOTE: for work. Hence, the event delivery thread is signaled if the
 *       MT-NOTE: very first event is added.
 *       MT-NOTE:
 *       MT-NOTE: If no event is due, i.e. the due date of the next event does lie
 *       MT-NOTE: ahead, the event delivery thread does wait until the next event
 *       MT-NOTE: does become due. Hence, the event delivery thread is signaled if
 *       MT-NOTE: an event is added which is due earlier. In addition the due
 *       MT-NOTE: date of the next event is set to the due date of the event just
 *       MT-NOTE: added
 */
void
te_add_event(te_event_t anEvent) {
   DENTER(EVENT_LAYER);

   SGE_ASSERT((anEvent != nullptr));

   uint64_t when = (ONE_TIME_EVENT == anEvent->mode) ? anEvent->when : (sge_get_gmt64() + anEvent->interval);

   auto le = ocs::tom::ElementPtr<ocs::tom::TE>::create();
   le->set<TE_when>(when);
   le->set<TE_type>(anEvent->type);
   le->set<TE_mode>(anEvent->mode);
   le->set<TE_interval>(anEvent->interval);
   le->set<TE_uval0>(anEvent->ulong_key_1);
   le->set<TE_uval1>(anEvent->ulong_key_2);
   le->set<TE_sval>(anEvent->str_key ? anEvent->str_key->c_str() : nullptr);

   DPRINTF("%s: (t:" sge_u32" w:" sge_u64" m:" sge_u32" s:%s)\n", __func__, static_cast<uint32_t>(anEvent->type),
           when, static_cast<uint32_t>(anEvent->mode), te_str_key_for_message(anEvent));

   sge_mutex_lock("event_control_mutex", __func__, __LINE__, &Event_Control.mutex);

   le->set<TE_seqno>(Event_Control.seq_no++);

   // Event_Control.list and .sort_order are process-lifetime globals that hold the list and the order raw (no
   // static owner), so the core inserts; without a list or an order the event is refused, as lInsertSorted()
   // refused it
   if (Event_Control.sort_order == nullptr || Event_Control.list == nullptr) {
      // the event is lost here - without this message nothing would ever tell that the timer
      // for e.g. a calendar state change was never armed
      ERROR(MSG_TE_CANNOTADDEVENT_US, static_cast<uint32_t>(anEvent->type),
            te_str_key_for_message(anEvent));
      le.reset();
   } else {
      try {
         ocs::tom::insert_sorted(Event_Control.list, le.get().raw(), Event_Control.sort_order);
      } catch (const ocs::tom::FieldException &e) {
         // a bound element or a duplicate key: a programming error that lInsertSorted() aborted on
         ocs::tom::legacy_abort("lInsertSorted", e);
      }
      // the event list owns the element now
      static_cast<void>(le.release());
   }

   if ((Event_Control.next == 0) || (when < Event_Control.next)) {
      Event_Control.next = when;

      pthread_cond_signal(&Event_Control.cond_var);

      DPRINTF("%s: did signal delivery thread!\n", __func__);
   }

   sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &Event_Control.mutex);

   DRETURN_VOID;
} /* te_add_event() */

/**
 * @brief Delete one time events
 *
 * Delete one or more one-time events. All one-time events which do EXACTLY
 * match the given arguments will be deleted from the event list.
 * If a timed event is scheduled for delivery, it will will NOT be deleted,
 * even if it does match the arguments. Such an event will be deleted after
 * event delivery has been finished.
 *
 * @param aType event type
 * @param aKey1 first numeric key
 * @param aKey2 second numeric key
 * @param strKey alphanumeric key
 *
 * @return number of events deleted
 *
 * @note MT-NOTE: te_delete_one_time_event() is MT safe.
 *       MT-NOTE:
 *       MT-NOTE: If a timed event has been deleted we need to signal the event
 *       MT-NOTE: delivery thread. This is because the event delivery thread
 *       MT-NOTE: maybe waiting until the just deleted event becomes due. Event
 *       MT-NOTE: deletion is communicated by setting 'Event_Control.deleted'
 *       MT-NOTE: to 'true'.
 */
int te_delete_one_time_event(te_type_t aType, uint32_t aKey1, uint32_t aKey2, const char *strKey) {
   DENTER(EVENT_LAYER);

   int ret;

   ret = te_delete_all_or_one_time_event(aType, aKey1, aKey2, strKey, false);
   DRETURN(ret);
}

/**
 * @brief Delete one time events
 *
 * Delete all one-time events of the given type.
 * If a timed event is scheduled for delivery, it will will NOT be deleted,
 * even if it does match the arguments. Such an event will be deleted after
 * event delivery has been finished.
 *
 * @param aType event type
 *
 * @return number of events deleted
 *
 * @note MT-NOTE: te_delete_all_one_time_event() is MT safe.
 *       MT-NOTE:
 *       MT-NOTE: If a timed event has been deleted we need to signal the event
 *       MT-NOTE: delivery thread. This is because the event delivery thread
 *       MT-NOTE: maybe waiting until the just deleted event becomes due. Event
 *       MT-NOTE: deletion is communicated by setting 'Event_Control.deleted'
 *       MT-NOTE: to 'true'.
 */
int te_delete_all_one_time_events(te_type_t aType) {
   DENTER(EVENT_LAYER);

   int ret;

   ret = te_delete_all_or_one_time_event(aType, 0, 0, nullptr, true);
   DRETURN(ret);
}


/**
 * @brief Return timed event due date
 *
 * @param anEvent timed event
 *
 * @return due date
 *
 * @note MT-NOTE: 'te_get_when()' is MT safe.
 */
uint64_t te_get_when(te_event_t anEvent) {
   DENTER(EVENT_LAYER);

   SGE_ASSERT(nullptr != anEvent);

   uint64_t res = anEvent->when;

   DRETURN(res);
} /* te_get_when() */

/**
 * @brief Return timed event type
 *
 * Return timed event type.
 *
 * @param anEvent timed event
 *
 * @return timed event type
 *
 * @note MT-NOTE: 'te_get_type()' is MT safe.
 */
te_type_t te_get_type(te_event_t anEvent) {
   DENTER(EVENT_LAYER);

   te_type_t res;

   SGE_ASSERT(nullptr != anEvent);

   res = anEvent->type;

   DRETURN(res);
} /* te_get_type() */

/**
 * @brief Return timed event first numeric key
 *
 * Return timed event first numeric key.
 *
 * @param anEvent timed event
 *
 * @return first numeric key
 *
 * @note MT-NOTE: 'te_get_first_numeric_key()' is MT safe.
 */
uint32_t te_get_first_numeric_key(te_event_t anEvent) {
   DENTER(EVENT_LAYER);

   uint32_t res = 0;

   SGE_ASSERT(nullptr != anEvent);

   res = anEvent->ulong_key_1;

   DRETURN(res);
} /* te_get_first_numeric_key() */

/**
 * @brief Return timed event second numeric key
 *
 * Return timed event second numeric key.
 *
 * @param anEvent timed event
 *
 * @return second numeric key
 *
 * @note MT-NOTE: 'te_get_second_numeric_key()' is MT safe.
 */
uint32_t te_get_second_numeric_key(te_event_t anEvent) {
   DENTER(EVENT_LAYER);

   uint32_t res = 0;

   SGE_ASSERT(nullptr != anEvent);

   res = anEvent->ulong_key_2;

   DRETURN(res);
} /* te_get_second_numeric_key() */

/**
 * @brief Return timed event alphanumeric key
 *
 * The key is not copied: the pointer refers into the event and is valid as long as the event exists and its
 * key is not changed - for an event handler, until the handler returns.
 *
 * @param anEvent timed event
 *
 * @return alphanumeric key, or nullptr if no key is set
 *
 * @note MT-NOTE: 'te_get_alphanumeric_key()' is MT safe.
 */
const char *te_get_alphanumeric_key(te_event_t anEvent) {
   DENTER(EVENT_LAYER);

   SGE_ASSERT(nullptr != anEvent);

   const char *res = anEvent->str_key ? anEvent->str_key->c_str() : nullptr;

   DRETURN(res);
}

/**
 * @brief One-time initialization
 *
 * Create timed event list. Set list sort order to be ascending event due
 * time. Create event handler table of initial size.
 *
 * @note MT-NOTE: te_init() is not MT safe
 *       MT-NOTE:
 *       MT-NOTE: This function must only be used as a one-time initialization
 *       MT-NOTE: function.
 */
void te_init() {
   DENTER(EVENT_LAYER);

   // a process-lifetime global: no owning handle, see event_control_t
   Event_Control.list = ocs::tom::ListPtr<ocs::tom::TE>::create("timed event list").release();
   // ascending due time, as the format "%I+" on TE_when gave it
   static constexpr ocs::tom::SortKey by_due_time[] = {{TE_when, true}};
   Event_Control.sort_order = ocs::tom::make_sort_order(ocs::tom::TE::descriptor.e, by_due_time);

   Handler_Tbl.list.clear();
   Handler_Tbl.list.reserve(TBL_INIT_SIZE);

   DRETURN_VOID;
} /* te_init() */

/**
 * @brief Shutdown event delivery thread
 *
 * Shutdown event delivery thread. Set event control structure 'exit' flag.
 * Wait until event delivery thread did terminate.
 *
 * @note MT-NOTE: 'te_shutdown()' is MT safe.
 *       MT-NOTE:
 *       MT-NOTE: 'pthread_once()' is called for symmetry reasons. This module
 *       MT-NOTE: will be initialized on demand, i.e. each function may be
 *       MT-NOTE: invoked without any prerequisite.
 */
void te_shutdown() {
   DENTER(EVENT_LAYER);

   Handler_Tbl.list.clear();
   Handler_Tbl.list.shrink_to_fit();

   DRETURN_VOID;
} /* te_shutdown() */

/**
 * @brief Check time
 *
 * Check if 'aTime' is a point in time BEFORE the last timed event has been
 * delivered. If so, adjust all pending timed events and set the time of
 * last event delivery to 'aTime'. In addition adjust due date of the next
 * event.
 *
 * @param aTime time value to check
 *
 * @note MT-NOTE: te_check_time() is NOT MT safe!
 *       MT-NOTE:
 *       MT-NOTE: It may only be called with 'Event_Control.mutex' locked!
 */
void te_check_time(uint64_t aTime) {
   DENTER(EVENT_LAYER);

   if (Event_Control.last > aTime) {
      uint64_t delta = Event_Control.last - aTime;

      WARNING(MSG_SYSTEM_SYSTEMHASBEENMODIFIEDXSECONDS_I, (int) sge_gmt64_to_gmt32(delta));

      for (const auto le : ocs::tom::List<ocs::tom::TE>::from_typed(Event_Control.list)) {
         le.set<TE_when>((le.get<TE_when>() - delta));
      }

      Event_Control.last = aTime;
      Event_Control.next -= delta;
   }

   DRETURN_VOID;
} /* te_check_time() */

/**
 * @brief Allocate new timed event
 *
 * Allocate and initialize a new timed event. The new event will be
 * initialized using the given list element.
 * The caller of this function is responsible to free the timed event
 * returned, using 'te_free_event()'.
 *
 * @param aListElem list element (TE)
 *
 * @return new timed event
 *
 * @note MT-NOTE: te_event_from_list_elem() is MT safe.
 */
te_event_t te_event_from_list_elem(const ocs::tom::ConstElement<ocs::tom::TE> aListElem) {
   DENTER(EVENT_LAYER);

   te_event_t ev = ocs::uti::create<te_event>();

   ev->when = aListElem.get<TE_when>();
   ev->interval = aListElem.get<TE_interval>();
   ev->type = (te_type_t) aListElem.get<TE_type>();
   ev->mode = (te_mode_t) aListElem.get<TE_mode>();
   ev->ulong_key_1 = aListElem.get<TE_uval0>();
   ev->ulong_key_2 = aListElem.get<TE_uval1>();
   ev->seq_no = aListElem.get<TE_seqno>();

   if (const char *str = aListElem.get<TE_sval>(); str != nullptr) {
      ev->str_key = str;
   }

   DRETURN(ev);
} /* te_event_from_list_elem() */

/**
 * @brief Scan event handler table and deliver event
 *
 * Scan event handler table for an event handler which does match the event
 * type of 'anEvent'. If 'anEvent' is of mode 'RECURRING_EVENT' it will be
 * added to the event list again after delivery, with its due date adjusted.
 *
 * @param anEvent event to deliver
 * @param monitor for monitoring qmaster threads
 *
 * @note MT-NOTE: te_scan_table_and_deliver() is MT safe.
 *       MT-NOTE:
 *       MT-NOTE: Do NOT invoke this function with 'Event_Control.mutex' locked!
 *       MT-NOTE: Otherwise a deadlock may occur due to recursive mutex locking.
 */
void te_scan_table_and_deliver(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(EVENT_LAYER);

   te_handler_t handler = nullptr;

   DPRINTF("%s: event (t:" sge_u32 " w:" sge_u64 " m:" sge_u32" s:%s)\n",
           __func__, static_cast<uint32_t>(anEvent->type), anEvent->when, static_cast<uint32_t>(anEvent->mode),
           te_str_key_for_message(anEvent));

   sge_mutex_lock("handler_table_mutex", __func__, __LINE__, &Handler_Tbl.mutex);

   for (const auto &entry : Handler_Tbl.list) {
      if (entry.type == anEvent->type) {
         handler = entry.handler;
         break;
      }
   }

   sge_mutex_unlock("handler_table_mutex", __func__, __LINE__, &Handler_Tbl.mutex);

   if (handler != nullptr) {
      handler(anEvent, monitor);
   } else {
      WARNING(MSG_SYSTEM_RECEIVEDUNKNOWNEVENT_I, anEvent->type);
   }

   if (RECURRING_EVENT == anEvent->mode) {
      anEvent->when = sge_get_gmt64() + anEvent->interval;

      DPRINTF("%s: reccuring event (t:" sge_u32 " w:" sge_u64" m:" sge_u32" s:%s)\n",
              __func__, static_cast<uint32_t>(anEvent->type), anEvent->when, static_cast<uint32_t>(anEvent->mode),
              te_str_key_for_message(anEvent));

      te_add_event(anEvent);
   }

   DRETURN_VOID;
} /* te_scan_table_and_deliver */
