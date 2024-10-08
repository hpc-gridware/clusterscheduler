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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <unistd.h>

#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"

#include "sge_qmaster_timed_event.h"

 
void calendar_event_handler(te_event_t anEvent, monitoring_t *monitor);
void signal_resend_event_handler(te_event_t anEvent, monitoring_t *monitor);
void job_resend_event_handler(te_event_t anEvent, monitoring_t *monitor);

static void test_delete_nonexistent_event();
static void test_add_one_time_event_without_handler();
static void test_delete_one_time_event();
static void test_delete_multiple_one_time_events();
static void test_one_time_event_delivery();
static void test_multiple_one_time_events_delivery();
static void test_recurring_event_delivery();
static void test_add_earlier_one_time_event();
static void test_add_earlier_recurring_event();

int main(int argc, char* argv[])
{
   DENTER_MAIN(TOP_LAYER, "test_sge_qmaster_timed_event");

   sge_prof_set_enabled(false);

   te_init();

   printf("%s: delete_nonexistent_event ----------------------------------\n", __func__);

   test_delete_nonexistent_event();

   printf("%s: add_one_time_event_without_hander -------------------------\n", __func__);

   test_add_one_time_event_without_handler();

   printf("%s: delete_one_time_event -------------------------------------\n", __func__);

   test_delete_one_time_event();

   printf("%s: delete_multiple_one_time_events ---------------------------\n", __func__);

   test_delete_multiple_one_time_events();

   printf("%s: one_time_event_delivery -----------------------------------\n", __func__);

   test_one_time_event_delivery();

   printf("%s: multiple_one_time_events_delivery -------------------------\n", __func__);

   test_multiple_one_time_events_delivery();

   printf("%s: recurring_event_delivery ----------------------------------\n", __func__);

   test_recurring_event_delivery();

   printf("%s: add_earlier_one_time_event --------------------------------\n", __func__);

   test_add_earlier_one_time_event();

   printf("%s: add_earlier_recurring_event -------------------------------\n", __func__);

   test_add_earlier_recurring_event();

   printf("%s: shutdown --------------------------------------------------\n", __func__);

   te_shutdown();

   DRETURN(0);
} /* main() */

void calendar_event_handler(te_event_t anEvent, monitoring_t *monitor)
{
   DENTER(TOP_LAYER);

   DPRINTF("%s: time:" sge_u32" when:" sge_u32"\n", __func__, time(nullptr), te_get_when(anEvent));

   DRETURN_VOID;
} /* calendar_event_handler() */

void signal_resend_event_handler(te_event_t anEvent, monitoring_t *monitor)
{
   DENTER(TOP_LAYER);

   DPRINTF("%s: time:" sge_u32" when:" sge_u32"\n", __func__, time(nullptr), te_get_when(anEvent));

   DRETURN_VOID;
} /* signal_resend_event_handler() */

void job_resend_event_handler(te_event_t anEvent, monitoring_t *monitor)
{
   DENTER(TOP_LAYER);

   DPRINTF("%s: time:" sge_u32" when:" sge_u32"\n", __func__, time(nullptr), te_get_when(anEvent));

   DRETURN_VOID;
} /* job_resend_event_handler() */

static void test_delete_nonexistent_event()
{
   te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, "no-event");

   sleep(2);
}

static void test_add_one_time_event_without_handler()
{
   te_event_t ev1;

   ev1 = te_new_event(time(nullptr), TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, "calendar_event-1");
   te_add_event(ev1);
   te_free_event(&ev1);

   sleep(3);

   return;
}

static void test_delete_one_time_event()
{
   te_event_t ev1;
   time_t when = time(nullptr) + 30;

   ev1 = te_new_event(when, TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, "calendar_event-2");
   te_add_event(ev1);
   te_free_event(&ev1);

   sleep(3);

   te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, "calendar_event-2");

   sleep(2);

   return;
}

static void test_delete_multiple_one_time_events()
{
   te_event_t ev1;
   time_t when1, when2 = 0;

   when1 = time(nullptr) + 10;
   ev1 = te_new_event(when1, TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, "calendar_event-3");
   te_add_event(ev1);
   te_free_event(&ev1);

   when1 = time(nullptr) + 20;
   ev1 = te_new_event(when1, TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, "calendar_event-3");
   te_add_event(ev1);
   te_free_event(&ev1);

   when2 = time(nullptr) + 30;
   ev1 = te_new_event(when2, TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, "calendar_event-3");
   te_add_event(ev1);
   te_free_event(&ev1);

   sleep(3);

   te_delete_one_time_event(TYPE_CALENDAR_EVENT, 0, 0, "calendar_event-3");

   sleep(10);

   return;
}

static void test_one_time_event_delivery()
{
   te_event_t ev1;

   te_register_event_handler(calendar_event_handler, TYPE_CALENDAR_EVENT);
   ev1 = te_new_event(time(nullptr), TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, "calendar_event-4");
   te_add_event(ev1);
   te_free_event(&ev1);

   sleep(2);

   return;
}

static void test_multiple_one_time_events_delivery()
{
   te_event_t ev1, ev2, ev3;

   te_register_event_handler(signal_resend_event_handler, TYPE_SIGNAL_RESEND_EVENT);
   te_register_event_handler(job_resend_event_handler, TYPE_JOB_RESEND_EVENT);

   ev1 = te_new_event(time(nullptr), TYPE_CALENDAR_EVENT, ONE_TIME_EVENT, 0, 0, "calendar_event-5");
   ev2 = te_new_event(time(nullptr), TYPE_SIGNAL_RESEND_EVENT, ONE_TIME_EVENT, 0, 0, "signal-resend-event-1");
   ev3 = te_new_event(time(nullptr), TYPE_JOB_RESEND_EVENT, ONE_TIME_EVENT, 0, 0, "job-resend-event-1");

   te_add_event(ev1);
   te_add_event(ev2);
   te_add_event(ev3);

   te_free_event(&ev1);
   te_free_event(&ev2);
   te_free_event(&ev3);

   sleep(4);

   return;
}

static void test_recurring_event_delivery()
{
   te_event_t ev1;

   ev1 = te_new_event(20, TYPE_SIGNAL_RESEND_EVENT, RECURRING_EVENT, 0, 0, "signal-resend-event-2");
   te_add_event(ev1);
   te_free_event(&ev1);

   sleep(45);

   return;
}

static void test_add_earlier_one_time_event()
{
   te_event_t ev1;

   ev1 = te_new_event(time(nullptr), TYPE_JOB_RESEND_EVENT, ONE_TIME_EVENT, 0, 0, "job-resend-event-2");
   te_add_event(ev1);
   te_free_event(&ev1);

   sleep(20);

   return;
}

static void test_add_earlier_recurring_event()
{
   te_event_t ev1;

   ev1 = te_new_event(5, TYPE_JOB_RESEND_EVENT, RECURRING_EVENT, 0, 0, "job-resend-event-3");
   te_add_event(ev1);
   te_free_event(&ev1);

   sleep(20);

   return;
}
