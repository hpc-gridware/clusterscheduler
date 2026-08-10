#pragma once
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
 * @brief The timer qmaster hangs all its periodic and deferred work on
 *
 * Anything qmaster has to do *later* - resend a signal a job ignored, run the
 * calendar, flush the reporting file, decay the share tree - is registered
 * here as a timed event rather than given a thread of its own. One thread
 * sleeps until the next event is due and dispatches it to the handler
 * registered for its type.
 *
 * That is why adding a periodic task means adding a #te_type_t and a handler,
 * and why nothing in qmaster polls.
 */

#include <ctime>
#include <sys/types.h>

#include <cinttypes>
#include "cull/cull_list.h"
#include "uti/sge_monitor.h"
#include "sgeobj/sge_daemonize.h"

enum {
   TBL_GROW_FACTOR = 2,/**< How much the handler table grows by when it is full */TBL_INIT_SIZE = 10/**< How many handlers the table holds to begin with */
};

/** @brief What a timed event is for
 *
 * Each value has exactly one handler, registered with
 * te_register_event_handler().
 */
typedef enum {
   TYPE_CALENDAR_EVENT = 4,            ///< A queue calendar changes state
   TYPE_SIGNAL_RESEND_EVENT,           ///< A signal a job has not reacted to is sent again
   TYPE_JOB_RESEND_EVENT,              ///< A job delivery an execution host has not confirmed is retried
   TYPE_RESCHEDULE_UNKNOWN_EVENT,      ///< Jobs on a host that stopped answering are rescheduled
   TYPE_SPOOLING_TRIGGER,              ///< Pending spool writes are flushed
   TYPE_REPORTING_TRIGGER,             ///< The reporting file is flushed
   TYPE_SHARELOG_TRIGGER,              ///< A share tree snapshot is written
   TYPE_HEARTBEAT_EVENT,               ///< The heartbeat file is bumped, so the shadow daemons see life
   TYPE_LOAD_VALUE_CLEANUP_EVENT,      ///< Load values older than their lifetime are dropped
   TYPE_AUTOMATIC_USER_CLEANUP_EVENT,  ///< Automatically created users that went idle are removed
   TYPE_REMOTE_EVENT_DELIVERY_EVENT,   ///< not used
   TYPE_JOB_NUMBER_EVENT,              ///< The next job number is spooled, so ids survive a restart
   TYPE_ACCOUNTING_TRIGGER,            ///< The accounting file is flushed
   TYPE_AR_ID_EVENT,                   ///< The next advance reservation id is spooled
   TYPE_AR_EVENT,                      ///< An advance reservation starts or ends
   TYPE_ENFORCE_LIMIT_EVENT,           ///< A resource quota limit is re-checked
   TYPE_SESSION_CLEANUP_EVENT,         ///< trigger that cleans up old sessions that have not been used for a longer time
   TYPE_SHARETREE_SPOOL_EVENT,         ///< CS-1239: drain ocs::SharetreeUsage FIFOs to the spool backend
   TYPE_SHARETREE_TICK_EVENT,          ///< CS-1239: periodic decay + batched USER_MOD/PROJECT_MOD/NEW_SHARETREE publish
   TYPE_FINISHED_JOBS_SWEEP_EVENT,     ///< CS-1908: periodic sweep pruning retained finished ja_tasks by time-OR-count
#if defined(OCS_WITH_OPENSSL)
   TYPE_SSL_CERT_RENEWAL_EVENT,        ///< The SSL certificate is renewed before it expires
#endif
} te_type_t;

/** @brief Whether an event fires once or keeps coming back */
typedef enum {
   ONE_TIME_EVENT = 1,   ///< Delivered once and then forgotten
   RECURRING_EVENT       ///< Re-armed with te_event::interval after every delivery
} te_mode_t;

/** @brief One thing to be done at a given time
 *
 * The three keys are what the handler is given to identify its subject - a job
 * id, a task id and a name - and also what
 * te_delete_one_time_event() matches on when an event has to be cancelled.
 */
struct te_event {
   uint64_t when;        ///< event delivery time (absolute timestamp in microseconds)
   uint64_t interval;    ///< event interval, if recurring event (relative in microseconds)
   te_type_t type;        ///< event type
   te_mode_t mode;        ///< event mode
   uint32_t ulong_key_1; ///< 1st numeric key
   uint32_t ulong_key_2; ///< 2nd numeric key
   const char *str_key;     ///< alphanumeric key
   uint32_t seq_no;      ///< event sequence number
};

/** @brief The event list and what the delivery thread waits on
 *
 * `deleted` exists because an event may be cancelled while the delivery thread
 * is asleep on it: the flag tells the thread to re-read the list rather than
 * trust the due date it went to sleep with.
 */
typedef struct {
   pthread_mutex_t mutex;      ///< used for mutual exclusion
   pthread_cond_t cond_var;   ///< used for waiting
   bool exit;       ///< true -> exit event delivery
   bool deleted;     ///< true -> at least one event has been deleted
   lList *list;       ///< timed event list
   lSortOrder *sort_order; ///< list sort order
   uint32_t seq_no;     ///< last added timed event sequence number
   uint64_t last;       ///< last time, event delivery has been checked
   uint64_t next;       ///< due date for next event, 0 -> event list is empty
} event_control_t;

typedef struct te_event *te_event_t;   ///< @copybrief te_event

/** @brief What is called when an event comes due
 *
 * Runs on the delivery thread, so a handler that blocks holds up every other
 * event.
 */
typedef void (*te_handler_t)(te_event_t, monitoring_t *monitor);

/** @brief One entry of the handler table: an event type and what handles it */
struct tbl_elem {
   te_type_t type;    ///< event type
   te_handler_t handler; ///< event handler
};

/** @brief The registered handlers, one per event type */
typedef struct {
   pthread_mutex_t mutex; ///< used for mutual exclusion
   int num;   ///< number of event handler in table
   int max;   ///< max number of handler, before resize
   struct tbl_elem *list;  ///< event handler list
} handler_tbl_t;

extern handler_tbl_t Handler_Tbl;      ///< The registered handlers

extern event_control_t Event_Control;  ///< The event list and its delivery thread

/* internal interface functions */

void te_check_time(uint64_t time);

void te_wait_empty();

te_event_t te_event_from_list_elem(const lListElem *aListElem);

void te_wait_next(te_event_t te, uint64_t now);

void te_scan_table_and_deliver(te_event_t anEvent, monitoring_t *monitor);

void te_init();

/* user interface */

extern void te_register_event_handler(te_handler_t, te_type_t);

extern te_event_t te_new_event(uint64_t, te_type_t, te_mode_t, uint32_t, uint32_t, const char *);

extern void te_free_event(te_event_t *);

extern void te_add_event(te_event_t);

extern int te_delete_one_time_event(te_type_t, uint32_t, uint32_t, const char *);

extern int te_delete_all_one_time_events(te_type_t aType);

extern void te_shutdown();

extern uint64_t te_get_when(te_event_t);

extern te_type_t te_get_type(te_event_t);

extern uint32_t te_get_first_numeric_key(te_event_t);

extern uint32_t te_get_second_numeric_key(te_event_t);

extern char *te_get_alphanumeric_key(te_event_t);
