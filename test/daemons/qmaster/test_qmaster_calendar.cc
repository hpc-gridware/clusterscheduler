/*___INFO__MARK_BEGIN_NEW__*/
/*************************************************************************
 *
 *  Copyright 2003 Sun Microsystems, Inc.
 *  Copyright 2023-2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include <cstdio>
#include <ctime>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/sge_qinstance_state.h"

#include "sge_c_gdi.h"
#include "sge_calendar_qmaster.h"
#include "msg_common.h"

typedef struct {
   const char *year_cal;    ///< year calendar definition
   const char *week_cal;    ///< week calendar definition
   const char *description; ///< human-readable description for test output
} cal_entry_t;

typedef struct {
   int       cal_nr;   ///< index into calendars[]
   struct tm now;      ///< simulated current date/time
   struct tm result1;  ///< expected time of first state change
   int       state1;   ///< expected current state
   struct tm result2;  ///< expected time of second state change
   int       state2;   ///< expected second state (-1 = no second entry)
} date_entry_t;

typedef struct {
   int       cal_nr;     ///< index into calendars[]
   struct tm start_time; ///< start of the time frame under test
   uint32_t  duration;   ///< duration of the time frame in seconds
   bool      open;       ///< expected result: calendar open during this frame
} time_frame_entry_t;

static cal_entry_t calendars[] = {
/* year calendar */  {"1.2.2004-1.3.2004=suspended", "NONE",
                      "queue is suspended in March 2004"},

                     {"1.2.2004-1.3.2004=off", "NONE",
                      "queue is off in March 2004"},

                     {"1.2.2004-1.4.2004=off 1.3.2004-1.5.2004=off", "NONE",
                      "queue is off from March till June 2004, using 2 calendar entries"},

                     {"1.2.2004-1.4.2004=suspended 1.3.2004-1.5.2004=off", "NONE",
                      "two overlapping calendar entries, one off, one suspended"},

                     {"1.2.2004-1.4.2004=9:0-18:0=suspended", "NONE",
                      "queue is suspended in March 2004 during the day"},

                     {"1.2.2004-1.4.2004=18:0-9:0=suspended", "NONE",
                      "queue is enabled in March 2004 during the day"},

                     {"1.2.2004-1.6.2004=18:0-9:0=suspended 1.3.2004-1.5.2004=suspended", "NONE",
                      "queue is supended during the night, and turned suspended for 2 month."},

/* no calendar */    {"NONE", "NONE",
                      "no calendar defined"},

/* week calendar */  {"NONE", "Mon-Sun=suspended",
                      "queue is always disabled"},

                     {"NONE", "Mon-Sun=09:00-18:00=suspended",
                      "queue is disabled during the day"},

                     {"NONE", "Mon-sun=18:00-09:00=suspended",
                      "queue is disabled during the night"},

                     {"NONE", "Mon,Wed,Fri=09:00-18:00=suspended",
                      "queue is disabled on Monday, Friday, and Wednesday during the day"},

                     {"NONE", "Mon-Wed=09:00-18:00=suspended Mon-Fri=suspended",
                      "queue is disabled on Monday till Wednesday during the day"},

/* mixed calendars */{"1.2.2004-1.3.2004=suspended", "Mon-Sun=09:00-18:00=suspended",
                      "queue is disabled during the day, except from 2/1/2004 till 3/1/2004. During that time it disabled for the whole day."},

                     {"24.12.2004-26.12.2004=on", "Mon-Fri=06:00-18:00=off Mon-Fri=09:00-18:00=suspended",
                      "queue is only enabled on the none working hours and Christmas"},

                     {"1.2.2004-1.3.2004=suspended", "Mon-Sun=suspended Mon-Sun=09:00-18:00=suspended",
                      "queue is always disabled"},

                     {"NONE", "Sun-Wed=on Wed-Sat=on",
                      "queue is always enabled"},

                     {"1.1.2004-1.2.2004=suspended 1.2.2004-1.3.2004=suspended 1.3.2004-1.4.2004=suspended 1.4.2004-30.4.2004=suspended 1.5.2004-1.6.2004=suspended", "NONE",
                      "queue is always disabled"},

                     {"NONE", "Mon-Wed=on Wed-Fri,Wed-Sat,Sun=on",
                      "queue is always enabled"},

                     {"1.2.2004-1.3.2004=on", "Mon-Wed=on Wed-Sun=on Mon-Sun=09:00-18:00=on",
                      "queue is always enabled"},

                     {"NONE", "09:00-18:00=suspended",
                      "queue is suspended from 9 to 6 every day"},

                     {"NONE", "Sun-Sat=suspended Wed-Fri=on",
                      "queue is always suspended except Wednesday till Friday"},

/* disabling queues */{"off",       "NONE", "queue is always off"},
                      {"suspended", "NONE", "queue is always suspended"},
                      {"NONE",      "off",  "queue is always off"},
                      {"NONE",      "suspended", "queue is always suspended"},

/* issue 1787 */      {"NONE", "mon=0:0:0-21:0:0", "queue is off every monday from 0 to 21 hours"},

/* end of definition */{nullptr, nullptr, nullptr}
};

/*
 * If no state change is expected, set result to {0,0,1,1,0,70,0,0,0} (time_t 0).
 * Time/date: sec, min, hour, mday (from 1), mon (from 0), year (since 1900), 0, 0, 0.
 * state2 == -1 means no second state-change entry is expected.
 */
static date_entry_t tests[] = {
   {0,  {0,0,0, 1,0,104, 0,0,0}, {0,0,0, 1,1,104, 0,0,0}, QI_DO_NOTHING,      {0,0,0, 2,2,104, 0,0,0}, QI_DO_CAL_SUSPEND},
   {0,  {0,0,0, 1,1,104, 0,0,0}, {0,0,0, 2,2,104, 0,0,0}, QI_DO_CAL_SUSPEND,  {0,0,1, 1,0, 70, 0,0,0}, QI_DO_NOTHING},
   {0,  {0,0,0, 2,2,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0}, QI_DO_NOTHING,      {0,0,1, 1,0, 70, 0,0,0}, -1},

   {1,  {0,0,0, 1,0,104, 0,0,0}, {0,0,0, 1,1,104, 0,0,0}, QI_DO_NOTHING,      {0,0,0, 2,2,104, 0,0,0}, QI_DO_CAL_DISABLE},
   {1,  {0,0,0, 1,1,104, 0,0,0}, {0,0,0, 2,2,104, 0,0,0}, QI_DO_CAL_DISABLE,  {0,0,1, 1,0, 70, 0,0,0}, QI_DO_NOTHING},
   {1,  {0,0,0, 2,2,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0}, QI_DO_NOTHING,      {0,0,1, 1,0, 70, 0,0,0}, -1},

   {2,  {0,0,0, 1,0,104, 0,0,0}, {0,0,0, 1,1,104, 0,0,0}, QI_DO_NOTHING,      {0,0,0, 2,4,104, 0,0,1}, QI_DO_CAL_DISABLE},
   {2,  {0,0,0, 1,1,104, 0,0,0}, {0,0,0, 2,4,104, 0,0,1}, QI_DO_CAL_DISABLE,  {0,0,1, 1,0, 70, 0,0,0}, QI_DO_NOTHING},
   {2,  {0,0,0, 1,2,104, 0,0,0}, {0,0,0, 2,4,104, 0,0,1}, QI_DO_CAL_DISABLE,  {0,0,1, 1,0, 70, 0,0,0}, QI_DO_NOTHING},
   {2,  {0,0,0, 1,3,104, 0,0,0}, {0,0,0, 2,4,104, 0,0,1}, QI_DO_CAL_DISABLE,  {0,0,1, 1,0, 70, 0,0,0}, QI_DO_NOTHING},
   {2,  {0,0,0, 2,4,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0}, QI_DO_NOTHING,      {0,0,1, 1,0, 70, 0,0,0}, -1},

   {3,  {0,0,0, 1,0,104, 0,0,0}, {0,0,0, 1,1,104, 0,0,0}, QI_DO_NOTHING,      {0,0,0, 2,3,104, 0,0,1}, QI_DO_CAL_SUSPEND},
   {3,  {0,0,0, 1,1,104, 0,0,0}, {0,0,0, 2,3,104, 0,0,1}, QI_DO_CAL_SUSPEND,  {0,0,0, 2,4,104, 0,0,1}, QI_DO_CAL_DISABLE},
   {3,  {0,0,0, 1,2,104, 0,0,0}, {0,0,0, 2,3,104, 0,0,1}, QI_DO_CAL_SUSPEND,  {0,0,0, 2,4,104, 0,0,1}, QI_DO_CAL_DISABLE},
   {3,  {0,0,0, 2,3,104, 0,0,0}, {0,0,0, 2,4,104, 0,0,1}, QI_DO_CAL_DISABLE,  {0,0,1, 1,0, 70, 0,0,0}, QI_DO_NOTHING},
   {3,  {0,0,0, 2,4,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0}, QI_DO_NOTHING,      {0,0,1, 1,0, 70, 0,0,0}, -1},

   {4,  {0,0, 0, 1,0,104, 0,0,0}, {0,0, 9, 1,1,104, 0,0,0}, QI_DO_NOTHING,     {0,0,18, 1,1,104, 0,0,0}, QI_DO_CAL_SUSPEND},
   {4,  {0,0, 0, 2,2,104, 0,0,0}, {0,0, 9, 2,2,104, 0,0,0}, QI_DO_NOTHING,     {0,0,18, 2,2,104, 0,0,0}, QI_DO_CAL_SUSPEND},
   {4,  {0,0,10, 2,2,104, 0,0,0}, {0,0,18, 2,2,104, 0,0,0}, QI_DO_CAL_SUSPEND, {0,0, 9, 3,2,104, 0,0,0}, QI_DO_NOTHING},
   {4,  {0,0,19, 2,2,104, 0,0,0}, {0,0, 9, 3,2,104, 0,0,0}, QI_DO_NOTHING,     {0,0,18, 3,2,104, 0,0,0}, QI_DO_CAL_SUSPEND},
   {4,  {0,0, 0, 2,4,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0},  QI_DO_NOTHING,     {0,0,1, 1,0, 70, 0,0,0}, -1},

   {5,  {0,0, 0, 1,0,104, 0,0,0}, {0,0,18, 1,1,104, 0,0,0}, QI_DO_NOTHING,     {0,0, 9, 2,1,104, 0,0,0}, QI_DO_CAL_SUSPEND},
   {5,  {0,0,20, 1,2,104, 0,0,0}, {0,0, 9, 2,2,104, 0,0,0}, QI_DO_CAL_SUSPEND, {0,0,18, 2,2,104, 0,0,0}, QI_DO_NOTHING},
   {5,  {0,0,10, 2,2,104, 0,0,0}, {0,0,18, 2,2,104, 0,0,0}, QI_DO_NOTHING,     {0,0, 9, 3,2,104, 0,0,0}, QI_DO_CAL_SUSPEND},
   {5,  {0,0, 0, 2,4,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0},  QI_DO_NOTHING,     {0,0,1, 1,0, 70, 0,0,0}, -1},

   {6,  {0,0, 0, 1,2,104, 0,0,0}, {0,0, 9, 2,4,104, 0,0,1}, QI_DO_CAL_SUSPEND, {0,0,18, 2,4,104, 0,0,1}, QI_DO_NOTHING},

   {7,  {0,0, 0, 1,2,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0},  QI_DO_NOTHING,     {0,0,1, 1,0, 70, 0,0,0}, -1},

   {8,  {0,0, 0, 1,2,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0},  QI_DO_CAL_SUSPEND, {0,0,1, 1,0, 70, 0,0,0}, -1},

   {9,  {0,0, 0, 1,1,104, 0,0,0}, {0,0, 9, 1,1,104, 0,0,0}, QI_DO_NOTHING,     {0,0,18, 1,1,104, 0,0,0}, QI_DO_CAL_SUSPEND},
   {9,  {0,0,10, 1,1,104, 0,0,0}, {0,0,18, 1,1,104, 0,0,0}, QI_DO_CAL_SUSPEND, {0,0, 9, 2,1,104, 0,0,0}, QI_DO_NOTHING},
   {9,  {0,0,20, 1,1,104, 0,0,0}, {0,0, 9, 2,1,104, 0,0,0}, QI_DO_NOTHING,     {0,0,18, 2,1,104, 0,0,0}, QI_DO_CAL_SUSPEND},

   {10, {0,0, 0, 1,1,104, 0,0,0}, {0,0, 9, 1,1,104, 0,0,0}, QI_DO_CAL_SUSPEND, {0,0,18, 1,1,104, 0,0,0}, QI_DO_NOTHING},
   {10, {0,0,20, 1,1,104, 0,0,0}, {0,0, 9, 2,1,104, 0,0,0}, QI_DO_CAL_SUSPEND, {0,0,18, 2,1,104, 0,0,0}, QI_DO_NOTHING},

   {11, {0,0, 0,22,8,104, 0,0,1}, {0,0, 9,22,8,104, 0,0,1}, QI_DO_NOTHING,     {0,0,18,22,8,104, 0,0,1}, QI_DO_CAL_SUSPEND},
   {11, {0,0,10,22,8,104, 0,0,1}, {0,0,18,22,8,104, 0,0,1}, QI_DO_CAL_SUSPEND, {0,0, 9,24,8,104, 0,0,1}, QI_DO_NOTHING},
   {11, {0,0,20,22,8,104, 0,0,1}, {0,0, 9,24,8,104, 0,0,1}, QI_DO_NOTHING,     {0,0,18,24,8,104, 0,0,1}, QI_DO_CAL_SUSPEND},
   {11, {0,0,20,24,8,104, 0,0,1}, {0,0, 9,27,8,104, 0,0,1}, QI_DO_NOTHING,     {0,0,18,27,8,104, 0,0,1}, QI_DO_CAL_SUSPEND},
   {11, {0,0,20,20,8,104, 0,0,1}, {0,0, 9,22,8,104, 0,0,1}, QI_DO_NOTHING,     {0,0,18,22,8,104, 0,0,1}, QI_DO_CAL_SUSPEND},

   {12, {0,0, 0,20,8,104, 0,0,1}, {0,0, 0,25,8,104, 0,0,1}, QI_DO_CAL_SUSPEND, {0,0, 0,27,8,104, 0,0,1}, QI_DO_NOTHING},

   {13, {0,0, 0,20,8,104, 0,0,1}, {0,0, 9,20,8,104, 0,0,1}, QI_DO_NOTHING,     {0,0,18,20,8,104, 0,0,1}, QI_DO_CAL_SUSPEND},
   {13, {0,0, 0, 2,1,104, 0,0,0}, {0,0, 0, 2,2,104, 0,0,0}, QI_DO_CAL_SUSPEND, {0,0, 9, 2,2,104, 0,0,0}, QI_DO_NOTHING},
   {13, {0,0,10, 1,0,104, 0,0,0}, {0,0,18, 1,0,104, 0,0,0}, QI_DO_CAL_SUSPEND, {0,0, 9, 2,0,104, 0,0,0}, QI_DO_NOTHING},

   {14, {0,0, 0,24,11,104, 0,0,0}, {0,0, 6,27,11,104, 0,0,0}, QI_DO_NOTHING,    {0,0, 9,27,11,104, 0,0,0}, QI_DO_CAL_DISABLE},
   {15, {0,0, 0, 1, 2,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0},   QI_DO_CAL_SUSPEND, {0,0,1, 1,0, 70, 0,0,0}, -1},
   {16, {0,0, 0, 1, 2,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0},   QI_DO_NOTHING,     {0,0,1, 1,0, 70, 0,0,0}, -1},
   {17, {0,0, 0, 2, 0,104, 0,0,0}, {0,0,0, 2, 5,104, 0,0,1},  QI_DO_CAL_SUSPEND, {0,0,1, 1,0, 70, 0,0,0}, QI_DO_NOTHING},
   {18, {0,0, 0, 1, 2,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0},   QI_DO_NOTHING,     {0,0,1, 1,0, 70, 0,0,0}, -1},
   {19, {0,0, 0, 1, 2,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0},   QI_DO_NOTHING,     {0,0,1, 1,0, 70, 0,0,0}, -1},

   {20, {0,0, 0, 1,1,104, 0,0,0}, {0,0, 9, 1,1,104, 0,0,0}, QI_DO_NOTHING,     {0,0,18, 1,1,104, 0,0,0}, QI_DO_CAL_SUSPEND},
   {20, {0,0,10, 1,1,104, 0,0,0}, {0,0,18, 1,1,104, 0,0,0}, QI_DO_CAL_SUSPEND, {0,0, 9, 2,1,104, 0,0,0}, QI_DO_NOTHING},
   {20, {0,0,20, 1,1,104, 0,0,0}, {0,0, 9, 2,1,104, 0,0,0}, QI_DO_NOTHING,     {0,0,18, 2,1,104, 0,0,0}, QI_DO_CAL_SUSPEND},

   {21, {0,0, 0,20,8,104, 0,0,1}, {0,0, 0,22,8,104, 0,0,1}, QI_DO_CAL_SUSPEND, {0,0, 0,25,8,104, 0,0,1}, QI_DO_NOTHING},

   {22, {0,0, 0, 1,2,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0}, QI_DO_CAL_DISABLE,  {0,0,1, 1,0, 70, 0,0,0}, -1},
   {23, {0,0, 0, 1,2,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0}, QI_DO_CAL_SUSPEND,  {0,0,1, 1,0, 70, 0,0,0}, -1},
   {24, {0,0, 0, 1,2,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0}, QI_DO_CAL_DISABLE,  {0,0,1, 1,0, 70, 0,0,0}, -1},
   {25, {0,0, 0, 1,2,104, 0,0,0}, {0,0,1, 1,0, 70, 0,0,0}, QI_DO_CAL_SUSPEND,  {0,0,1, 1,0, 70, 0,0,0}, -1},

   {26, {0,0, 0, 1,2,104, 0,0,0}, {0,0,21, 1,2,104, 0,0,0}, QI_DO_CAL_DISABLE,  {0,0, 0, 8,2,104, 0,0,0}, QI_DO_NOTHING},
   {26, {0,0,10, 1,2,104, 0,0,0}, {0,0,21, 1,2,104, 0,0,0}, QI_DO_CAL_DISABLE,  {0,0, 0, 8,2,104, 0,0,0}, QI_DO_NOTHING},
   {26, {0,0,22, 1,2,104, 0,0,0}, {0,0, 0, 8,2,104, 0,0,0}, QI_DO_NOTHING,      {0,0,21, 8,2,104, 0,0,0}, QI_DO_CAL_DISABLE},
   {26, {0,0,12, 3,2,104, 0,0,0}, {0,0, 0, 8,2,104, 0,0,0}, QI_DO_NOTHING,      {0,0,21, 8,2,104, 0,0,0}, QI_DO_CAL_DISABLE},

   {-1, {0,0,0, 0,0,104, 0,0,0}, {0,0,0, 0,0,104, 0,0,0}, -1, {0,0,0, 0,0,104, 0,0,0}, -1}
};

static time_frame_entry_t time_frame_tests[] = {
/* year calendar */  {0, {0,0,0, 1,0,104, 0,0,0}, 3600, true},
                     {0, {0,30,23, 31,0,104, 0,0,0}, 3600, false},
                     {0, {0,0,12, 31,0,105, 0,0,0}, 3600, true},
                     {1, {0,0,0, 1,0,104, 0,0,0}, 3600, true},
                     {1, {0,30,23, 31,0,104, 0,0,0}, 3600, false},
                     {1, {0,0,12, 31,0,105, 0,0,0}, 3600, true},
                     {2, {0,0,0, 1,0,104, 0,0,0}, 3600, true},
                     {2, {0,30,23, 31,0,104, 0,0,0}, 3600, false},
                     {2, {0,0,12, 31,0,104, 0,0,0}, 6048000, false}, /* 70 days */
/* no calendar */    {7, {0,30,23, 31,0,104, 0,0,0}, 3600, true},
/* week calendar */  {8, {0,0,0, 1,0,104, 0,0,0}, 3600, false},
                     {8, {0,30,23, 31,0,104, 0,0,0}, 3600, false},
                     {8, {0,0,12, 31,0,105, 0,0,0}, 3600, false},
                     {9, {1,0,18, 12,5,105, 0,0,1}, 53999, false}, /* 15 hours minus one second */
                     {9, {1,0,18, 12,5,105, 0,0,1}, 53998, true},  /* 15 hours minus two seconds */
                     {9, {0,0,18, 12,5,105, 0,0,1}, 53999, false}, /* 15 hours minus one second */
/* mixed calendars */{13, {1,0,18, 2,2,104, 0,0,0}, 3600, true},
                     {13, {1,0,18, 15,1,104, 0,0,0}, 3600, false},
                     {-1, {0,0,0, 0,0,104, 0,0,0}, 0, false}
};

static void printDateError(time_t *when, struct tm *expected)
{
   struct tm res;
   struct tm *result = localtime_r(when, &res);

   printf("wrong change date:\n");
   printf("expected: sec:%d min:%d hour:%d mday:%d mon:%d year:%d wday:%d yday:%d isdst:%d\n",
      expected->tm_sec, expected->tm_min, expected->tm_hour,
      expected->tm_mday, expected->tm_mon, expected->tm_year,
      expected->tm_wday, expected->tm_yday, expected->tm_isdst);
   printf("got     : sec:%d min:%d hour:%d mday:%d mon:%d year:%d wday:%d yday:%d isdst:%d\n",
      result->tm_sec, result->tm_min, result->tm_hour,
      result->tm_mday, result->tm_mon, result->tm_year,
      result->tm_wday, result->tm_yday, result->tm_isdst);
}

static lListElem *createCalObject(cal_entry_t *calendar)
{
   monitoring_t monitor;
   lListElem *sourceCal = nullptr;
   lListElem *destCal = nullptr;
   lList *answerList = nullptr;

   sge_monitor_init(&monitor, "cal_test", NONE_EXT, NO_WARNING, NO_ERROR, nullptr);

   sourceCal = lCreateElem(CAL_Type);
   lSetString(sourceCal, CAL_name, "test");
   lSetString(sourceCal, CAL_year_calendar, calendar->year_cal);
   lSetString(sourceCal, CAL_week_calendar, calendar->week_cal);

   destCal = lCreateElem(CAL_Type);

   if (0 != calendar_mod(nullptr, nullptr, &answerList, destCal, sourceCal, 1, "", "", nullptr,
                         ocs::gdi::Command::NONE, ocs::gdi::SubCommand::NONE, &monitor)) {
      lWriteListTo(answerList, stdout);
      lFreeElem(&destCal);
      lFreeList(&answerList);
   }

   lFreeElem(&sourceCal);
   sge_monitor_free(&monitor);
   return destCal;
}

// returns true when stateObject matches expected state and time
static bool check_state_change(lListElem *stateObject, uint32_t state, struct tm *time, int elem_nr)
{
   if (lGetUlong(stateObject, CQU_state) != state) {
      printf("wrong state in state list (elem %d): expected %d, got %d\n",
             elem_nr, (int)state, (int)lGetUlong(stateObject, CQU_state));
      return false;
   }
   time_t expected = mktime(time);
   time_t result = sge_gmt64_to_time_t(lGetUlong64(stateObject, CQU_till));
   if (result != expected) {
      printf("state list elem %d: ", elem_nr);
      printDateError(&result, time);
      return false;
   }
   return true;
}

// returns true when the full state-change list matches the expected entries
static bool check_state_change_list(date_entry_t *t, lList *state_changes)
{
   int nr;
   if (t->state2 != -1) {
      if ((nr = lGetNumberOfElem(state_changes)) != 2) {
         printf("wrong number of elements in state change list: expected 2, got %d\n", nr);
         return false;
      }
      lListElem *state = lFirstRW(state_changes);
      if (!check_state_change(state, t->state1, &t->result1, 1)) {
         return false;
      }
      state = lNextRW(state);
      return check_state_change(state, t->state2, &t->result2, 2);
   } else {
      if ((nr = lGetNumberOfElem(state_changes)) != 1) {
         printf("wrong number of elements in state change list: expected 1, got %d\n", nr);
         return false;
      }
      lListElem *state = lFirstRW(state_changes);
      return check_state_change(state, t->state1, &t->result1, 1);
   }
}

// runs one date/state scenario; prints diagnostics on failure; returns true on pass
static bool run_date_test(date_entry_t *t, cal_entry_t *cal)
{
   lListElem *destCal = createCalObject(cal);
   if (destCal == nullptr) {
      printf("createCalObject failed\n");
      return false;
   }

   bool ok = false;
   time_t when = 0;
   time_t now = mktime(&t->now);
   lList *state_changes_list = nullptr;

   uint64_t when64 = sge_time_t_to_gmt64(when);
   uint64_t now64 = sge_time_t_to_gmt64(now);
   uint32_t current_state = calender_state_changes(destCal, &state_changes_list, &when64, &now64);
   when = sge_gmt64_to_time_t(when64);

   if ((int)current_state != t->state1) {
      printf("wrong state: expected %d, got %d\n", t->state1, (int)current_state);
   } else if (when != mktime(&t->result1)) {
      printDateError(&when, &t->result1);
   } else {
      ok = check_state_change_list(t, state_changes_list);
   }

   lFreeList(&state_changes_list);
   lFreeElem(&destCal);
   return ok;
}

// runs one time-frame scenario; returns true when open/closed matches expectation
static bool run_time_frame_test(time_frame_entry_t *t, cal_entry_t *cal)
{
   lListElem *destCal = createCalObject(cal);
   if (destCal == nullptr) {
      printf("createCalObject failed\n");
      return false;
   }

   uint64_t start_time = sge_time_t_to_gmt64(mktime(&t->start_time));
   bool result = calendar_open_in_time_frame(destCal, start_time, sge_gmt32_to_gmt64(t->duration));
   bool ok = (t->open == result);
   if (!ok) {
      printf("wrong state for time frame: expected %d, got %d\n", (int)t->open, (int)result);
   }

   lFreeElem(&destCal);
   return ok;
}

static int s_fail = 0;

#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

int main(int /*argc*/, char * /*argv*/[])
{
   DENTER_MAIN(TOP_LAYER, "test_qmaster_calendar");
   component_set_daemonized(true);
   lInit(nmv);

   char label[256];
   int id = 1;

   printf("\n--- date/state tests ---\n");
   for (int i = 0; tests[i].cal_nr != -1; i++) {
      int c = tests[i].cal_nr;
      snprintf(label, sizeof(label), "cal[%d] %d.%d.%d %02d:%02d - %s",
               c,
               tests[i].now.tm_mday, tests[i].now.tm_mon + 1,
               tests[i].now.tm_year + 1900,
               tests[i].now.tm_hour, tests[i].now.tm_min,
               calendars[c].description);
      CHECK(id++, label, run_date_test(&tests[i], &calendars[c]));
   }

   printf("\n--- time-frame tests ---\n");
   for (int i = 0; time_frame_tests[i].cal_nr != -1; i++) {
      int c = time_frame_tests[i].cal_nr;
      snprintf(label, sizeof(label), "tf[%d] %d.%d.%d %02d:%02d dur=%us expect=%s - %s",
               c,
               time_frame_tests[i].start_time.tm_mday,
               time_frame_tests[i].start_time.tm_mon + 1,
               time_frame_tests[i].start_time.tm_year + 1900,
               time_frame_tests[i].start_time.tm_hour,
               time_frame_tests[i].start_time.tm_min,
               time_frame_tests[i].duration,
               time_frame_tests[i].open ? "open" : "closed",
               calendars[c].description);
      CHECK(id++, label, run_time_frame_test(&time_frame_tests[i], &calendars[c]));
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}