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
 * @brief Calendars: when a queue disables or suspends itself on its own
 *
 * A calendar is written as a `year` and a `week` specification, both parsed
 * here into CULL lists. The scheduler then asks
 * #calender_state_changes what the queue's state is now and when it changes
 * next, so the timed event thread knows when to look again.
 *
 * @see sge_calendar.h
 */

#include <cstdio>
#include <cstring>
#include <cctype>
#include <ctime>
#include <optional>
#include <tuple>
#include <utility>

#include "cull/cull_list.h"

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_attr.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_calendar.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_ASTR.h"
#include "sgeobj/tom/ocs_tom_CA.h"
#include "sgeobj/tom/ocs_tom_TM.h"
#include "sgeobj/tom/ocs_tom_TMR.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_CQU.h"

#include "msg_qmaster.h"


/// The tokens a calendar specification is made of
enum {
   DOT = 1,              ///< a `.`
   COLON,                ///< a `:`
   EQUAL_SIGN,           ///< an `=`
   MINUS,                ///< a `-`
   COMMA,                ///< a `,`
   SPACE,                ///< whitespace
   NUMBER,               ///< a number
   STRING,               ///< a word
   NO_TOKEN,             ///< the input was exhausted
   ERR_TOKEN             ///< not a token; the input could not be tokenized
};

/// One entry of the calendar parser's token table
typedef struct {
   int token;         ///< the token id the text stands for
   const char *text;  ///< the text an administrator writes
} token_set_t;


static token_set_t statev[] = {
   { QI_DO_ENABLE,   "on" },
   { QI_DO_DISABLE,  "off" },
   { QI_DO_SUSPEND, "suspended" },
   { -1, nullptr },
};

/**
 * @brief Orders two calendar entries
 *
 * @param t1 the first entry
 * @param t2 the second entry
 * @return negative, zero or positive as `t1` sorts before, with or after `t2`
 */
using cmp_func_t = int (*)(ocs::tom::ConstElement<ocs::tom::TM> t1, ocs::tom::ConstElement<ocs::tom::TM> t2);

static char old_error[1000];
static char store[1000];
static int number;
static int token_is_valid = 0;
static char parse_error[MAX_STRING_SIZE];

/* parsing */
static std::optional<ocs::tom::ListPtr<ocs::tom::CA>>
disabled_year_list(lList **alpp, const char *s, const char *cal_name);

static std::optional<ocs::tom::ListPtr<ocs::tom::CA>>
disabled_week_list(lList **alpp, const char *s, const char *cal_name);

static std::pair<int, time_t>
state_at(time_t now, ocs::tom::ConstList<ocs::tom::CA> ycal, ocs::tom::ConstList<ocs::tom::CA> wcal);

static int scan(const char *s, token_set_t token_set[]);

static void join_wday_range(ocs::tom::List<ocs::tom::TMR> week_day);
static void extend_wday_range(ocs::tom::List<ocs::tom::TMR> week_day);

static char *get_string();

static int get_number();

static void eat_token();

static char *save_error();

static int cheap_scan(char *s, token_set_t tokenv[], int n, const char *name);

static ocs::tom::ElementPtr<ocs::tom::CA> disabled_year_entry();

static ocs::tom::ListPtr<ocs::tom::TMR> year_day_range_list();

static ocs::tom::ElementPtr<ocs::tom::TMR> year_day_range();

static ocs::tom::ElementPtr<ocs::tom::TM> year_day();

static int month(int *);

static int day(int *);

static int year(int *);

static int tm_yday_cmp(ocs::tom::ConstElement<ocs::tom::TM> t1, ocs::tom::ConstElement<ocs::tom::TM> t2);

static int tm_wday_cmp(ocs::tom::ConstElement<ocs::tom::TM> t1, ocs::tom::ConstElement<ocs::tom::TM> t2);

static int normalize_range_list(ocs::tom::List<ocs::tom::TMR> rl, cmp_func_t cmp_func);

static bool in_range_list(ocs::tom::ConstElement<ocs::tom::TM> tm, ocs::tom::ConstList<ocs::tom::TMR> rl,
                          cmp_func_t cmp_func);

static int in_range(ocs::tom::ConstElement<ocs::tom::TM> tm, ocs::tom::ConstElement<ocs::tom::TMR> r,
                    cmp_func_t cmp_func);

static ocs::tom::ListPtr<ocs::tom::TMR> daytime_range_list();

static ocs::tom::ElementPtr<ocs::tom::TMR> daytime_range();

static ocs::tom::ListPtr<ocs::tom::TMR> full_daytime_range();

static ocs::tom::ListPtr<ocs::tom::TMR> full_weekday_range();

static void split_daytime_range(ocs::tom::List<ocs::tom::TMR> dtrl, ocs::tom::Element<ocs::tom::TMR> tmr);

static ocs::tom::ElementPtr<ocs::tom::TM> daytime();

static int hour(int *);

static int minute(int *);

static int seconds(int *);

static int action(int *sp);

static int range_number(int min, int max, int *ip, const char *name);

static int tm_daytime_cmp(ocs::tom::ConstElement<ocs::tom::TM> t1, ocs::tom::ConstElement<ocs::tom::TM> t2);

static std::tuple<uint32_t, time_t, uint32_t>
is_week_entry_active(ocs::tom::ConstElement<ocs::tom::TM> tm, ocs::tom::ConstElement<ocs::tom::CA> week_entry,
                     int rec_count);

static std::pair<uint32_t, time_t>
is_year_entry_active(ocs::tom::ConstElement<ocs::tom::TM> tm, ocs::tom::ConstElement<ocs::tom::CA> year_entry);

static std::pair<time_t, bool>
compute_limit(bool today, bool active, ocs::tom::ConstList<ocs::tom::TMR> year_time,
              ocs::tom::ConstList<ocs::tom::TMR> week_time, ocs::tom::ConstList<ocs::tom::TMR> day_time,
              ocs::tom::ConstElement<ocs::tom::TM> now);

static ocs::tom::ElementPtr<ocs::tom::CA> disabled_week_entry();

static ocs::tom::ListPtr<ocs::tom::TMR> week_day_range_list();

static ocs::tom::ElementPtr<ocs::tom::TMR> week_day_range();

static void split_wday_range(ocs::tom::List<ocs::tom::TMR> wdrl, ocs::tom::Element<ocs::tom::TMR> tmr);

static ocs::tom::ElementPtr<ocs::tom::TM> week_day();

static std::pair<uint32_t, time_t>
calendar_get_current_state_and_end(ocs::tom::ConstElement<ocs::tom::CAL> cep, time_t now);

/**
 * @brief Computes the state a year and a week calendar give at a time, and when this state ends
 *
 * Here we have to answer two questions:
 * 1. Which one is the actual state?
 *    If both this day and the actual daytime are in an entry then this entry
 *    is active and the entry's state is returned. In case there is more than
 *    one active entry the 'hardest' is dominant (disabled < suspended < enabled).
 * 2. When will this state end?
 *    For all active entries we compute the end of the range. For all inactive
 *    entries we compute the start of the range. We iterate through all these
 *    ranges beginning with that range which is closest to 'now' in the future.
 *    In some cases the state does not change but the range which is
 *    responsible for that state changes: `mon-fri=9-18 fri=18-19`
 *
 * The parsed calendars look like this:
 * ```
 * year                          week
 *    CA_yday_range_list            CA_wday_range_list
 *       TMR_begin                     TMR_begin
 *          TM_mday, TM_mon, TM_year      TM_wday
 *       TMR_end                       TMR_end
 *          TM_mday, TM_mon, TM_year      TM_wday
 *    CA_daytime_range_list         CA_daytime_range_list
 *       TMR_begin                     TMR_begin
 *          TM_hour, TM_min, TM_sec       TM_hour, TM_min, TM_sec
 *       TMR_end                       TMR_end
 *          TM_hour, TM_min, TM_sec       TM_hour, TM_min, TM_sec
 *    CA_state                      CA_state
 * ```
 *
 * Future enhancement: the computation of the next event could be improved.
 * Actually state_at() returns 24 o'clock of this day as the end of this state
 * also when this state is valid beyond this day.
 *
 * @param now the time to look at
 * @param ycal the parsed year calendar (CA_Type), may be empty
 * @param wcal the parsed week calendar (CA_Type), may be empty
 * @return the state (QI_DO_ENABLE, QI_DO_SUSPEND or QI_DO_DISABLE) and the next time this calendar has to be
 *         checked again, 0 if there is none
 */
static std::pair<int, time_t>
state_at(const time_t now, const ocs::tom::ConstList<ocs::tom::CA> ycal, const ocs::tom::ConstList<ocs::tom::CA> wcal) {
   DENTER(TOP_LAYER);
   struct tm *tm_now;
   uint32_t w_is_active, y_is_active;
   bool have_week_cal = false;
   time_t limit;
   time_t temp_next_event = 0;
   struct tm res;
   uint32_t state = 0, next_state;

   /* convert time_t format into struct tm format */
   tm_now = localtime_r(&now, &res);

   /* cullify struct tm */
   auto tm = ocs::tom::ElementPtr<ocs::tom::TM>::create();
   cullify_tm(*tm, tm_now);

   DPRINTF("now: sec: %d min: %d hour: %d mday: %d mon: %d year:%d wday: %d yday: %d isdst: %d\n",
         tm_now->tm_sec,
         tm_now->tm_min,
         tm_now->tm_hour,
         tm_now->tm_mday,
         tm_now->tm_mon,
         tm_now->tm_year,
         tm_now->tm_wday,
         tm_now->tm_yday,
         tm_now->tm_isdst);

   /* ycal */
   for (const auto yc : ycal) {
      std::tie(y_is_active, limit) = is_year_entry_active(*tm, yc);
      if (state != y_is_active || state == QI_DO_NOTHING) {
         state |= y_is_active;

         if (temp_next_event == 0 || (limit != 0 && temp_next_event > limit)) {
            temp_next_event= limit;
         }
      }
      else {
         if (temp_next_event == 0 || (limit != 0 && temp_next_event < limit)) {
            temp_next_event= limit;
         }
      }
   }

   if (!state) {
      int counter = 0;
      const std::size_t max = wcal.size();
      bool *visited = (bool*) sge_malloc(max * sizeof(bool));
      bool isOverlapping;

      memset(visited, false, max * sizeof(bool));

      /* Week calendar */
      /* we can have overlapping ranges, which will not result in state changes. This code tries
         to figure out, when a range overlaps and we face no state change. */
      do {
         counter = 0;
         isOverlapping = false;

         for (const auto wc : wcal) {
            have_week_cal = true;
            std::tie(w_is_active, limit, next_state) = is_week_entry_active(*tm, wc, 0);
            if (w_is_active > 0) {
               state |= w_is_active;
            }

            if (limit != 0 && state == next_state && temp_next_event >= limit) {
               if (!visited[counter]) {
                  isOverlapping = true;
                  visited[counter] = true;
                  temp_next_event += 1;
                  tm_now = localtime_r(&temp_next_event, &res);
                  cullify_tm(*tm, tm_now);

                  temp_next_event = 0;
                  std::tie(w_is_active, limit, next_state) = is_week_entry_active(*tm, wc, 0);
                  if (w_is_active > 0) {
                     state |= w_is_active;
                  }
               } else {
                  temp_next_event = 0;
                  isOverlapping = false;
                  break;
               }
            }

            if ((state != next_state) && (temp_next_event == 0 || ((limit != 0) && (temp_next_event > limit)))) {
               temp_next_event= limit;
            }
            counter++;
         }
      } while(isOverlapping);

      sge_free(&visited);
   }

   DPRINTF("got state %d from %s calendar. Now: %lld Next event: %lld\n",
           state, have_week_cal ? "week" : "year", static_cast<long long>(now),
           static_cast<long long>(temp_next_event));

   tm.reset();

   int result = QI_DO_ENABLE;
   if ((state & QI_DO_ENABLE)) {
      result = QI_DO_ENABLE;
   } else if ((state & QI_DO_SUSPEND)) {
      result = QI_DO_SUSPEND;
   } else if ((state & QI_DO_DISABLE)) {
      result = QI_DO_DISABLE;
   }

   DRETURN(std::make_pair(result, temp_next_event));
}

/**
 * @brief Computes the current queue state and the next change
 *
 * Computes the current queue state based on its calendar setting. It also figures
 * out how long this state lasts and when the next change will be. The function
 * for the next change has a short comming, that it will generate an state end, when
 * the current day changes.
 * Therefore this function calles itself recursivly, if such an event happens. It can
 * only happen, when the callendar contains a state changes during a day. In Theory, it
 * should not call itself more than 1 time. With the extra call, it should have found a
 * new state change on the next day, or the day, when the next change comes.
 *
 * @param tm TM_Type - represeting now (time)
 * @param week_entry CA_Type - calendar entries for the week setup
 * @param rec_count current recoursion level
 *
 * @return the current state, the next state change (0, if there is no further state change) and the
 *         state after that change (0, if there is no further state change)
 *
 * @note MT-NOTE: is_week_entry_active() is MT safe
 */
static std::tuple<uint32_t, time_t, uint32_t>
is_week_entry_active(const ocs::tom::ConstElement<ocs::tom::TM> tm,
                     const ocs::tom::ConstElement<ocs::tom::CA> week_entry, int rec_count) {
   DENTER(TOP_LAYER);

   uint32_t state;
   uint32_t next_state;
   bool in_wday_range, in_daytime_range = false;


   /* compute state */
   if ((in_wday_range=in_range_list(tm, ocs::tom::get<CA_wday_range_list>(week_entry), tm_wday_cmp))
     && (in_daytime_range=in_range_list(tm, ocs::tom::get<CA_daytime_range_list>(week_entry), tm_daytime_cmp))) {
       /* DPRINTF(("in_range_list(wday) = %d in_range_list(daytime) = %d state = %d\n",
                    in_wday_range, in_daytime_range, lGetUlong(week_entry, CA_state))); */
      state = ocs::tom::get<CA_state>(week_entry);
      next_state = 0;
   } else {
      /* DPRINTF("in_range_list(wday) = %d in_range_list(daytime) = %d\n", in_wday_range, in_daytime_range); */
      state = 0;
      next_state = ocs::tom::get<CA_state>(week_entry);

   }

   auto [limit, is_end_of_the_day_reached] =
      compute_limit(in_wday_range, in_daytime_range, nullptr, ocs::tom::get<CA_wday_range_list>(week_entry),
                    ocs::tom::get<CA_daytime_range_list>(week_entry), tm);

   if (is_end_of_the_day_reached) {
      struct tm *tm_now;
      struct tm res;

      /* we have to add a second to get into the next time slot */
      limit++;

      /* convert time_t format into struct tm format */
      tm_now = localtime_r(&limit, &res);

      /* cullify struct tm */
      auto new_tm= ocs::tom::ElementPtr<ocs::tom::TM>::create();
      cullify_tm(*new_tm, tm_now);

      /* more than 2 recursion should never be needed, we allow 4 to be save.*/
      if (rec_count < 4) {
         std::tie(state, limit, next_state) = is_week_entry_active(*new_tm, week_entry, rec_count++);
      }
      else {
         ERROR(SFNMAX, MSG_CALENDAR_CALCTERMINATED);
      }

   }

   /* there is no other state change */
   if (limit == 0) {
      next_state = 0;
   }

   DRETURN(std::make_tuple(state, limit, next_state));
}

/**
 * @brief Computes the current queue state and the next change
 *
 * Computes the current queue state based on its calendar setting. It also figures
 * out how long this state lasts and when the next change will be. The function
 * for the next change has a short comming, that it will generate an state end, when
 * the current day changes.
 * Therefore this function calles itself recursivly, if such an event happens. It can
 * only happen, when the callendar contains a state changes during a day. In Theory, it
 * should not call itself more than 1 time. With the extra call, it should have found a
 * new state change on the next day, or the day, when the next change comes.
 * This function has problem, when two calendar entries overlap themselfs and one has
 * day changes, the otherone has not. It will report those day changes, even though there
 * are none. It makse a mistake in figuring out, which calendar setting is the more important
 * one.
 *
 * @param tm TM_Type - represeting now (time)
 * @param year_entry CA_Type - calendar entry of the year setup
 *
 * @return the current state and the next state change (0, if there is no further state change)
 *
 * @note MT-NOTE: is_year_entry_active() is MT safe
 */
static std::pair<uint32_t, time_t>
is_year_entry_active(const ocs::tom::ConstElement<ocs::tom::TM> tm,
                     const ocs::tom::ConstElement<ocs::tom::CA> year_entry) {
   DENTER(TOP_LAYER);

   uint32_t state;
   bool in_yday_range, in_daytime_range = false;

   /* compute state */
   if ((in_yday_range=in_range_list(tm, ocs::tom::get<CA_yday_range_list>(year_entry), tm_yday_cmp))
     && (in_daytime_range=in_range_list(tm, ocs::tom::get<CA_daytime_range_list>(year_entry), tm_daytime_cmp))) {
      DPRINTF("in_range_list(yday) = %d in_range_list(daytime) = %d state = %d\n",
            in_yday_range, in_daytime_range, ocs::tom::get<CA_state>(year_entry));
      state = ocs::tom::get<CA_state>(year_entry);
   } else {
      DPRINTF("in_range_list(yday) = %d in_range_list(daytime) = %d\n", in_yday_range, in_daytime_range);
      state = in_daytime_range?QI_DO_ENABLE:0;
   }

   auto [limit, is_end_of_the_day_reached] =
      compute_limit(in_yday_range, in_daytime_range, ocs::tom::get<CA_yday_range_list>(year_entry), nullptr,
                    ocs::tom::get<CA_daytime_range_list>(year_entry), tm);

   if (is_end_of_the_day_reached) {
      struct tm *tm_now;
      struct tm res;
      DPRINTF("trying the next time slot\n");
      /* we have to add a second to get into the next time slot */
      limit++;
      time_t next_day = limit;

      /* convert time_t format into struct tm format */
      tm_now = localtime_r(&limit, &res);

      /* cullify struct tm */
      auto new_tm= ocs::tom::ElementPtr<ocs::tom::TM>::create();
      cullify_tm(*new_tm, tm_now);

      auto [next_day_state, next_day_limit] = is_year_entry_active(*new_tm, year_entry);

      /* The state does not change for the rest of the day, that is why we look at the next day.
         If the next day starts in a different state then the change happens right at midnight
         and that is the limit we are looking for - the limit the recursion reported belongs to
         the state of the next day and would be too late. Otherwise the state indeed continues
         and both the state and its limit are the ones of the next day. */
      if (next_day_state != state) {
         limit = next_day;
      } else {
         state = next_day_state;
         limit = next_day_limit;
      }

   }

   DRETURN(std::make_pair(state, limit));
}


/**
 * @brief Calendar states and calendar state list
 *
 * Computes the current state and generates a calendar state list. Right now it
 * only does it for the current state and the next state. But it could be extended
 * to handle more states.
 * Based on the shortcomings of the methods it is using, it has to check, wether they
 * reported a state change and if not, call the methods again with a new time stamp. It
 * is starting with now and goes into the future, based on the reported state changes.
 *
 * @param cep calendar (CAL_Type); an empty handle gives state 0, no next change and no list
 * @param now64 nullptr for the current time, or the time to start from (only for the test program)
 *
 * @return the new state (QI_DO_NOTHING, QI_DO_CAL_DISABLE, QI_DO_CAL_SUSPEND), when the next change will
 *         be (0 if never) and the state change list (CQU_Type)
 *
 * @note MT-NOTE: calender_state_changes() is MT safe
 */
std::tuple<uint32_t, uint64_t, ocs::tom::ListPtr<ocs::tom::CQU>>
calender_state_changes(const ocs::tom::ConstElement<ocs::tom::CAL> cep, const uint64_t *now64) {
   DENTER(TOP_LAYER);

   time_t temp_when = 0;
   time_t temp_now= 0;
   time_t when1 = 0;
   uint32_t state0 = 0;
   uint32_t state1 = 0;
   uint32_t state2 = 0;
   uint64_t when64 = 0;
   time_t now = 0;
   if (now64 != nullptr) {
      now = sge_gmt64_to_time_t(*now64);
   } else {
      now = sge_gmt64_to_time_t(sge_get_gmt64());
   }

   if (cep == nullptr) {
      DRETURN(std::make_tuple(state0, when64, ocs::tom::ListPtr<ocs::tom::CQU>()));
   }

   /* build queue state change list */
   std::tie(state0, temp_when) = calendar_get_current_state_and_end(cep, now);
   when64 = sge_time_t_to_gmt64(temp_when);

   /* calculate the next state shift. */
   if (temp_when != 0) {
      int state_changes = 0;
      const int max_state_changes = 60; /* we want to limit this calculation to about month and for that we
                                          assume 2 state changes per day. This limit is only needed for a
                                          scheduler configuration like:
                                          year cal: "NONE", week cal: "Mon-Wed=09:00-18:00=suspended Mon-Fri=suspended"
                                          year cal: 1.2.2004-1.4.2004=off 1.3.2004-1.5.2004=off
                                        */

      /* we do have to do the calculation at least once, but there are cases, in which the
        calendar_get_current_state_and_end function does not return the correct state switch
        (overlapping calendar configurations), therefor we have to run it multiple times. */
      do {
         when64 = sge_time_t_to_gmt64(temp_when);
         temp_now = temp_when + 1;
         std::tie(state1, temp_when) = calendar_get_current_state_and_end(cep, temp_now);
         state_changes++;
      } while ((temp_when != 0) && (state0 == state1) && (state_changes < max_state_changes));

      if (state0 == state1) {
         when64 = sge_time_t_to_gmt64(temp_when);
      }

      when1 = temp_when;
      /* some problem with the next interval */
      if (when1 != 0) {
         do {
            when1 = temp_when;
            temp_now = temp_when + 1;
            std::tie(state2, temp_when) = calendar_get_current_state_and_end(cep, temp_now);
            state_changes++;
         } while ((temp_when != 0) && (state2 == state1) && (state_changes < max_state_changes));
      }

      if (state1 == state2) {
         when1 = temp_when;
      }
   }

   auto state_changes_list = ocs::tom::ListPtr<ocs::tom::CQU>::create("state_changes");
   auto state_change = ocs::tom::ElementPtr<ocs::tom::CQU>::create();

   ocs::tom::set<CQU_state>(state_change, state0);
   ocs::tom::set<CQU_till>(state_change, when64);
   state_changes_list->push_back(std::move(state_change));

   /* extend queue state change list */
   if (when64 != 0) {
      auto state_change = ocs::tom::ElementPtr<ocs::tom::CQU>::create();
      ocs::tom::set<CQU_state>(state_change,state1);
      ocs::tom::set<CQU_till>(state_change, sge_time_t_to_gmt64(when1));
      state_changes_list->push_back(std::move(state_change));
   }

   DRETURN(std::make_tuple(state0, when64, std::move(state_changes_list)));
}

/**
 * @brief calender_state_changes() for an untyped caller
 *
 * @param cep calendar (CAL_Type), checked; nullptr gives 0 and leaves the out-parameters alone
 * @param state_changes_list out: the state change list (CQU_Type); nullptr gives 0
 * @param when64 out: when the next change will be, or 0
 * @param now64 in/out: nullptr, or the current time (only for the test program); rounded to whole seconds
 *
 * @return as calender_state_changes()
 */
uint32_t calender_state_changes(const lListElem *cep, lList **state_changes_list, uint64_t *when64, uint64_t *now64) {
   if (cep == nullptr || state_changes_list == nullptr) {
      return 0;
   }

   auto [state, when, changes] = calender_state_changes(ocs::tom::ConstElement<ocs::tom::CAL>(cep), now64);
   *when64 = when;
   if (now64 != nullptr) {
      *now64 = sge_time_t_to_gmt64(sge_gmt64_to_time_t(*now64));
   }
   *state_changes_list = changes.release();
   return state;
}


/**
 * @brief Compute the next state change
 *
 * This function computes the next state change based on the three booleans:
 * today, active, and is_full_day.
 * There are some special calendar states, which result in special code to
 * handle thus states. They are all special to the week calendar.
 * - A week calendar can specify a range beyond the week borders. For example
 *   Wed-Mon. This will be broken down to two structures:
 *    - Wed-Sat (day 3 till 6)
 *    - Sun-Mon. (day 0 till 1)
 *   Since it is one range, the actual break down will look as follows:
 *    - Wed-Mon (day 3 till 8)
 *    - Sun-Mon (day 0 till 1)
 *   The first range, which is too big, allows us to recognize the whole range,
 *   thus we are not issuing a state change at 0 a.m. on Sunday but 0 a.m on
 *   Tuesday.
 * - An other special case is a calendar, which disables a queue for a whole week.
 *  There is special code to recognize that.
 *
 * @param today does the state change happen today?
 * @param active is a calendar entry active?
 * @param year_time year_time structure or nullptr, if week_time is set
 * @param week_time week_time strucutre or nullptr, if year_time is set
 * @param day_time day_time changes, if the exist
 * @param now a now cull representation
 *
 * @return time of the next change or end of the day, and whether the end of the day is reached
 *
 * @note MT-NOTE: compute_limit() is MT safe
 */
static std::pair<time_t, bool>
compute_limit(bool today, const bool active, const ocs::tom::ConstList<ocs::tom::TMR> year_time,
              const ocs::tom::ConstList<ocs::tom::TMR> week_time, const ocs::tom::ConstList<ocs::tom::TMR> day_time,
              const ocs::tom::ConstElement<ocs::tom::TM> now) {
   DENTER(TOP_LAYER);

   ocs::tom::ElementPtr<ocs::tom::TM> lep;
   bool end_of_day = false;
   bool is_end_of_day_reached = false;
   struct tm tm_limit;
   time_t limit;
   bool is_full_day = false;
   bool is_new_cal_entry = true;
   /* the time the daytime ranges are compared with: now, or the next calendar state change, which is then
      held by new_now_copy */
   ocs::tom::ConstElement<ocs::tom::TM> new_now = now;
   ocs::tom::ElementPtr<ocs::tom::TM> new_now_copy;

   if (day_time == nullptr) {
      DPRINTF("no day time calendar is set ");
      DRETURN(std::make_pair(static_cast<time_t>(0), false));
   }

   if (year_time != nullptr && week_time != nullptr) {
      DPRINTF("year and week calendar are set. It should only be one of them");
      DRETURN(std::make_pair(static_cast<time_t>(0), false));
   }

   if (year_time == nullptr && week_time == nullptr) {
      DPRINTF("we have no a calendar time definition. It is most likely set to \"off\" or \"suspended\"");
      DRETURN(std::make_pair(static_cast<time_t>(0), false));
   }

   { /* should only be one entry in case that the calendar is valid for whole days*/
      const auto first_range = day_time.front();
      const auto end = ocs::tom::get<TMR_end>(first_range).front();
      const auto begin = ocs::tom::get<TMR_begin>(first_range).front();

      if ( (ocs::tom::get<TM_sec>(end) == 0)   &&
           (ocs::tom::get<TM_min>(end) == 0)   &&
           (ocs::tom::get<TM_hour>(end) == 24) &&
           (ocs::tom::get<TM_sec>(begin) == 0) &&
           (ocs::tom::get<TM_min>(begin) == 0) &&
           (ocs::tom::get<TM_hour>(begin) == 0)) {
         is_full_day = true;
      }
   }

   /* do we look at a full day calendar or is the next entry not today? We then have to figure out the
      exect next date, when the calendar will be active /inactive again */
   if (is_full_day || !today) {
      if (active) { /* a calendar is active */
         if (year_time != nullptr) {
            for (const auto range : year_time) {
               if (in_range(now, range, tm_yday_cmp)) {
                  lep = ocs::tom::get<TMR_end>(range).front().clone();
                  if (lep == nullptr) {
                      lep = ocs::tom::get<TMR_begin>(range).front().clone();
                  }
                  break;
               }
            }
         }
         else if (week_time != nullptr) {
            bool is_allways_inactive = true;

            for (const auto range : week_time) {
               const auto endList = ocs::tom::get<TMR_end>(range);
               const auto beginList = ocs::tom::get<TMR_begin>(range);
               if (is_full_day && (endList != nullptr) && (beginList != nullptr)
                   && (ocs::tom::get<TM_wday>(endList.front()) - ocs::tom::get<TM_wday>(beginList.front()) == 6)) {
                   is_allways_inactive &= true; /* the calendar is disabled for the whole week. */
               }
               else {
                  is_allways_inactive &= false;
               }

               if (in_range(now, range, tm_wday_cmp)) {
                  auto end = endList.front();
                  uint32_t day;

                  /* we might only have one day specified. If so, the beginList contains the end */
                  if (end == nullptr) {
                     end = beginList.front();
                  }

                  day = ocs::tom::get<TM_mday>(now);

                  day += (ocs::tom::get<TM_wday>(end) - ocs::tom::get<TM_wday>(now));
                  lep = now.clone();
                  ocs::tom::set<TM_mday>(lep, day);
                  break;
               }
            }
            /* the calendar is disabled for the whole week. No need to compute anything further */
            if (is_allways_inactive) {
               DRETURN(std::make_pair(static_cast<time_t>(0), false));
            }
         }
         else {
            DPRINTF("year_time and week_time calendar are set, this should not be. ");
         }
         ocs::tom::set<TM_sec>(lep, 59); /* we cannot have 24:0:0. That would be equal to 0:0:0 and therefor be the */
         ocs::tom::set<TM_min>(lep, 59); /* beginning of the day and not its end */
         ocs::tom::set<TM_hour>(lep, 23);
         end_of_day=true;
      }
      else { /* a calendar is inactive */
         if (year_time != nullptr) {   /* year calenar */
            for (const auto range : year_time) {
               const auto begin = ocs::tom::get<TMR_begin>(range).front();
               if (tm_yday_cmp(now, begin) < 0 &&
                  (!lep || tm_yday_cmp(*lep, begin) > 0)) {
                  lep = begin.clone();
               }
            }
            if (lep == nullptr) {
               is_new_cal_entry = false;
            }
         }
         else if (week_time != nullptr) { /* week calendar */
            bool is_next_week = false;
            for (const auto range : week_time) {
               const auto begin = ocs::tom::get<TMR_begin>(range).front();
               if (tm_wday_cmp(now, begin) < 0 &&
                  (!lep || tm_wday_cmp(*lep, begin) > 0)) {
                  lep = begin.clone();
               }
            }
            if (lep == nullptr) {
               is_next_week = true;
               for (const auto range : week_time) {
                  const auto begin = ocs::tom::get<TMR_begin>(range).front();
                  if (!lep || tm_wday_cmp(*lep, begin) > 0) {
                     lep = begin.clone();
                  }
               }
            }
            if (is_next_week) {
               int days = ocs::tom::get<TM_mday>(now);
               days += (7 - ocs::tom::get<TM_wday>(now)) + ocs::tom::get<TM_wday>(lep);
               ocs::tom::set<TM_mday>(lep, days);
            }
            else {
               ocs::tom::set<TM_mday>(lep, ocs::tom::get<TM_mday>(now) +
                                           (ocs::tom::get<TM_wday>(lep) - ocs::tom::get<TM_wday>(now)));
            }
            ocs::tom::set<TM_mon>(lep, ocs::tom::get<TM_mon>(now));
            ocs::tom::set<TM_year>(lep, ocs::tom::get<TM_year>(now));
         }
         else {      /* error in the data structure. should not happen */
            DPRINTF("year_time and week_time calendar are not set");
         }
      }

      if (lep != nullptr) { /* we do not know about summer or winter time, let the system choose */
         ocs::tom::set<TM_isdst>(lep, -1);
      }

      if (is_new_cal_entry) {
         uncullify_tm(*lep, &tm_limit);
         if (is_full_day) {/* a full day calendar, nothing else is needed. */
            lep.reset();
         }
         else {
            today = true; /* we have to enable the calendar for today */
            new_now_copy = std::move(lep); /* set the next calendar state change as today */
            new_now = *new_now_copy;
         }

      }
   }

   /* the current calendar changes the state during the day, we have to compute the exact time, when
      that happens */
   if (!is_full_day && new_now != nullptr && today) {

      if (active) {
         /* inside of range - seek nearest end of range */
         /* seek end of active daytime range */
         for (const auto range : day_time) {
            if (in_range(new_now, range, tm_daytime_cmp)) {
               lep = ocs::tom::get<TMR_end>(range).front().clone();
               break;
            }
         }
      } else {
         /* outside of range - seek nearest begin of range.
            A day starts at 0:0:0 o'clock, therefore we have to check if we hit the start of the
            range right away or if we are smaller. This matters whenever new_now was synthesized
            above as 0:0:0 of a day in the future: a range that begins at 0:0 is the next state
            change on that day and a strict comparison would skip it, leaving the calendar without
            any further state change at all.
            A range that begins at the very daytime of the real now cannot show up here, because
            in_range() includes both ends of a range - we would be inside of it and "active" would
            be set. */
         for (const auto range : day_time) {
            const auto begin = ocs::tom::get<TMR_begin>(range).front();
            if (tm_daytime_cmp(new_now, begin) <= 0 &&
               (!lep || tm_daytime_cmp(*lep, begin) > 0)) {
               lep = begin.clone();
            }
         }
      }


      /* it might happen, that we are already to late for todays state shift, though we assume
         a state shift at the end of the day and record it. The function will be called again,
         to compute the state shift for the "next" day */
      if (!lep) {
         DPRINTF("reached end of the day\n");
         is_end_of_day_reached = true;
         lep = new_now.clone();
         ocs::tom::set<TM_hour>(lep, 24);
         ocs::tom::set<TM_min>(lep, 0);
         ocs::tom::set<TM_sec>(lep, 0);
      }

      /* convert from cull struct tm into time_t */
      end_of_day = (ocs::tom::get<TM_hour>(lep)==24) ? true : false;
      if (end_of_day) {
         ocs::tom::set<TM_hour>(lep, 23);
         ocs::tom::set<TM_min>(lep, 59);
         ocs::tom::set<TM_sec>(lep, 59);
      }
      uncullify_tm(new_now, &tm_limit);
      tm_limit.tm_hour = ocs::tom::get<TM_hour>(lep);
      tm_limit.tm_min = ocs::tom::get<TM_min>(lep);
      tm_limit.tm_sec = ocs::tom::get<TM_sec>(lep);
      lep.reset();
      is_new_cal_entry = true;
   }

   if (is_new_cal_entry) {
#if 0
      DPRINTF(("sec:%d min:%d hour:%d mday:%d mon:%d year:%d wday:%d yday:%d isdst:%d\n",
         tm_limit.tm_sec,
         tm_limit.tm_min,
         tm_limit.tm_hour,
         tm_limit.tm_mday,
         tm_limit.tm_mon,
         tm_limit.tm_year,
         tm_limit.tm_wday,
         tm_limit.tm_yday,
         tm_limit.tm_isdst));
#endif
      limit = mktime(&tm_limit);

      DPRINTF("limit: " sge_u64 "\n", static_cast<uint64_t>(limit));
      if (end_of_day) {
         limit += 1;
      }
   }
   else { /* the current state will last for ever */
      DPRINTF("no new calendar entry\n");
      limit = 0;
   }

   DRETURN(std::make_pair(limit, is_end_of_day_reached));

}

/**
 * @brief Joins overlapping ranges of a range list
 *
 * @param rl the range list (TMR_Type)
 * @param cmp_func the order of the range bounds
 * @return 0
 */
static int normalize_range_list(const ocs::tom::List<ocs::tom::TMR> rl, const cmp_func_t cmp_func) {
   DENTER(TOP_LAYER);

/*    lListElem *t1, *t2, *t3, *t4; */
/*    int i1, i2, i3, i4; */

/*    i1 = i2 = i3 = i4 = -1; */

   for (const auto r : rl) {

      auto r1 = ocs::tom::get<TMR_begin>(r).front();
      auto r2 = ocs::tom::get<TMR_end>(r).front();

      for (auto q = r.next(); q; q = q.next()) {

         const auto q1 = ocs::tom::get<TMR_begin>(q).front();
         const auto q2 = ocs::tom::get<TMR_end>(q).front();

         /* overlapping ranges ? */
         if (in_range(r1, q, cmp_func)         || /* r1 in q */
             (r2 && in_range(r2, q, cmp_func)) || /* r2 in q */
             in_range(q1, r, cmp_func)         || /* q1 in r */
             (q2 && in_range(q2, r, cmp_func)))   /* q2 in r */ {

/*             DPRINTF("%d %d %d %d\n", i1,i2,i3,i4); */
/*             t1=lFirst(lGetList(r, TMR_begin)); */
/*             t2=lFirst(lGetList(r, TMR_end)); */
/*             t3=lFirst(lGetList(q, TMR_begin)); */
/*             t4=lFirst(lGetList(q, TMR_end)); */

/*            DPRINTF(("overlapping ranges %d:%d:%d-%d:%d:%d and %d:%d:%d-%d:%d:%d\n",
               lGetUlong(t1, TM_hour),
               lGetUlong(t1, TM_min),
               lGetUlong(t1, TM_sec),

               lGetUlong(t2, TM_hour),
               lGetUlong(t2, TM_min),
               lGetUlong(t2, TM_sec),

               lGetUlong(t3, TM_hour),
               lGetUlong(t3, TM_min),
               lGetUlong(t3, TM_sec),

               lGetUlong(t4, TM_hour),
               lGetUlong(t4, TM_min),
               lGetUlong(t4, TM_sec)));*/

            /* combine both overlapping ranges */
            /* r1 = min(r1, q1) */
            if (cmp_func(r1, q1) > 0) {
               ocs::tom::swap<TMR_begin, TMR_begin>(r, q);
            }

            /* r2 = max(r2, q2) */
            if ( (q2 && !r2) || cmp_func(r2, q2)<0) {
               ocs::tom::swap<TMR_end, TMR_end>(r, q);
            }

/*             t1=lFirst(lGetList(r, TMR_begin)); */
/*             t2=lFirst(lGetList(r, TMR_end)); */

/*            DPRINTF(("resulting range %d:%d:%d-%d:%d:%d\n",
               lGetUlong(t1, TM_hour),
               lGetUlong(t1, TM_min),
               lGetUlong(t1, TM_sec),

               lGetUlong(t2, TM_hour),
               lGetUlong(t2, TM_min),
               lGetUlong(t2, TM_sec))); */

            /* remove q */
            rl.erase(q);

            /* prepare next loop iteration */
            q = r;

            /* r1, r2 are no longer valid after swapping */
            r1 = ocs::tom::get<TMR_begin>(r).front();
            r2 = ocs::tom::get<TMR_end>(r).front();
         }
      }
   }

   DRETURN(0);
}

/**
 * @brief Is a time in one of the ranges of a range list?
 *
 * @param tm the time (TM_Type)
 * @param rl the range list (TMR_Type); an empty handle contains every time
 * @param cmp_func the order of the times
 * @return true if the time is in a range or the list handle is empty
 */
static bool in_range_list(const ocs::tom::ConstElement<ocs::tom::TM> tm, const ocs::tom::ConstList<ocs::tom::TMR> rl,
                          const cmp_func_t cmp_func) {
   DENTER(TOP_LAYER);

   if (!rl) {
      DRETURN(true);
   }

   for (const auto r : rl) {
      if (in_range(tm, r, cmp_func)) {
         DRETURN(true);
      }
   }

   DRETURN(false);
}

/**
 * @brief Is a time in a range?
 *
 * A range without an end holds only its begin.
 *
 * @param tm the time (TM_Type)
 * @param r the range (TMR_Type)
 * @param cmp_func the order of the times
 * @return 1 if the time is in the range, 0 otherwise
 */
static int in_range(const ocs::tom::ConstElement<ocs::tom::TM> tm, const ocs::tom::ConstElement<ocs::tom::TMR> r,
                    const cmp_func_t cmp_func) {
   DENTER(TOP_LAYER);

   const auto t1 = ocs::tom::get<TMR_begin>(r).front();
   const auto t2 = ocs::tom::get<TMR_end>(r).front();

   if (t2) {
      if (cmp_func(t1, tm)<=0 && cmp_func(tm, t2) <= 0) { /* mon-fri */
#if 0
      DPRINTF(("%d:%d:%d is in range %d:%d:%d-%d:%d:%d\n",
         lGetUlong(tm, TM_hour),
         lGetUlong(tm, TM_min),
         lGetUlong(t1, TM_sec),

         lGetUlong(t1, TM_hour),
         lGetUlong(t1, TM_min),
         lGetUlong(t1, TM_sec),

         lGetUlong(t2, TM_hour),
         lGetUlong(t2, TM_min),
         lGetUlong(t2, TM_sec)));
#endif
         DRETURN(1);
      }

   } else {
      if (!cmp_func(t1, tm)) { /* must be equal for hit */
         DRETURN(1);
      }
   }

   DRETURN(0);
}

/**
 * @brief Parses a year calendar
 *
 * disabled_year_list := disabled_year_entry[<space>disabled_year_entry]
 *
 * @param alpp receives the syntax error
 * @param s the year calendar specification; nullptr or "none" for no calendar
 * @param cal_name the name of the calendar, for the error message
 * @return the parsed calendar (CA_Type), an empty handle for no calendar, std::nullopt on a syntax error
 */
static std::optional<ocs::tom::ListPtr<ocs::tom::CA>>
disabled_year_list(lList **alpp, const char *s, const char *cal_name) {
   DENTER(TOP_LAYER);

   static token_set_t token_set[] = {
      { DOT,             "." },
      { COLON,           ":" },
      { EQUAL_SIGN,      "=" },
      { MINUS,           "-" },
      { COMMA,           "," },
      { SPACE,           " " },
      { STRING,          nullptr },
      { NUMBER,          nullptr },
      { 0,               nullptr }
   };

   ocs::tom::ListPtr<ocs::tom::CA> cal;

   if (!s || !strcasecmp(s, "none")) {
      DRETURN(std::move(cal));
   }

   scan(s, token_set);

   {
      auto calep = disabled_year_entry();
      if (!calep) {
         goto ERROR;
      }

      cal = ocs::tom::ListPtr<ocs::tom::CA>::create("year list");
      cal->push_back(std::move(calep));
   }
   while (scan(nullptr, nullptr) == SPACE) {
      eat_token();

      while (scan(nullptr, nullptr) == SPACE) {
         eat_token();
      }

      auto calep = disabled_year_entry();
      if (!calep) {
         goto ERROR;
      }
      cal->push_back(std::move(calep));
   }

   /* complain about still unused tokens */
   if (scan(nullptr, nullptr)!=NO_TOKEN) {
      snprintf(parse_error, sizeof(parse_error), MSG_TOKEN_UNRECOGNIZEDSTRING_S , get_string());
      goto ERROR;
   }

   DRETURN(std::move(cal));

ERROR:
   cal.reset();
   snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ANSWER_ERRORINDISABLYEAROFCALENDARXY_SS, save_error(), cal_name);
   answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
   DRETURN(std::nullopt);
}

/**
 * @brief Parses one entry of a year calendar
 *
 * disabled_year_entry := year_day_range_list[=daytime_range_list][=state]
 *
 * @return the entry (CA_Type), an empty handle on a syntax error
 */
static ocs::tom::ElementPtr<ocs::tom::CA> disabled_year_entry() {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::TMR> ydrl;
   ocs::tom::ListPtr<ocs::tom::TMR> dtrl;
   int state = QI_DO_DISABLE;

   if (scan(nullptr, nullptr)==NUMBER) {
      ydrl = year_day_range_list();
      if (!ydrl)
         goto ERROR;

      if (scan(nullptr, nullptr)!=EQUAL_SIGN)
         goto SUCCESS;
      eat_token();
   }

   if (scan(nullptr, nullptr)==NUMBER) {
      dtrl = daytime_range_list();
      if (!dtrl)
         goto ERROR;
      normalize_range_list(*dtrl, tm_daytime_cmp);
      if (scan(nullptr, nullptr)!=EQUAL_SIGN)
         goto SUCCESS;
      eat_token();
   }

   if (scan(nullptr, nullptr)==STRING) {
      if (action(&state)) {
         goto ERROR;
      }
   } else {
      snprintf(parse_error, sizeof(parse_error), SFNMAX, MSG_ANSWER_GOTEQUALWITHOUTDAYTIMERANGEORSTATE );
      goto ERROR;
   }

SUCCESS:
   {
      auto cal = ocs::tom::ElementPtr<ocs::tom::CA>::create();
      ocs::tom::set<CA_yday_range_list>(cal, std::move(ydrl));
      if (!dtrl)
         dtrl = full_daytime_range();
      ocs::tom::set<CA_daytime_range_list>(cal, std::move(dtrl));
      DPRINTF("state = %d\n", state);
      ocs::tom::set<CA_state>(cal, state);
      DRETURN(cal);
   }

ERROR:
   DRETURN(nullptr);
}

/**
 * @brief A daytime range list of the whole day, 0-24
 *
 * @return the range list (TMR_Type)
 */
static ocs::tom::ListPtr<ocs::tom::TMR> full_daytime_range() {
   DENTER(TOP_LAYER);

   auto dtrl = ocs::tom::ListPtr<ocs::tom::TMR>::create("full day");

/*    DPRINTF("FULL_DAYTIME_RANGE\n"); */

   auto tmr = ocs::tom::ElementPtr<ocs::tom::TMR>::create();
   ocs::tom::emplace_back_in<TMR_begin, TM_hour>(tmr, 0);
   ocs::tom::emplace_back_in<TMR_end, TM_hour>(tmr, 24);

   dtrl->push_back(std::move(tmr));

   DRETURN(dtrl);
}

/**
 * @brief A week day range list of the whole week, sunday-saturday
 *
 * @return the range list (TMR_Type)
 */
static ocs::tom::ListPtr<ocs::tom::TMR> full_weekday_range() {
   DENTER(TOP_LAYER);

   auto dtrl = ocs::tom::ListPtr<ocs::tom::TMR>::create("full week");

/*    DPRINTF("FULL_WEEKDAY_RANGE\n"); */

   auto tmr = ocs::tom::ElementPtr<ocs::tom::TMR>::create();
   ocs::tom::emplace_back_in<TMR_begin, TM_wday>(tmr, 0);
   ocs::tom::emplace_back_in<TMR_end, TM_wday>(tmr, 6);

   dtrl->push_back(std::move(tmr));

   DRETURN(dtrl);
}


/**
 * @brief Parses a list of year day ranges
 *
 * year_day_range_list := year_day_range[,year_day_range]
 *
 * @return the range list (TMR_Type), an empty handle on a syntax error
 */
static ocs::tom::ListPtr<ocs::tom::TMR> year_day_range_list() {
   DENTER(TOP_LAYER);

   auto tmr = year_day_range();
   if (!tmr) {
      DRETURN(nullptr);
   }
   auto ydrl = ocs::tom::ListPtr<ocs::tom::TMR>::create("year_day_range_list");
   ydrl->push_back(std::move(tmr));

   while (scan(nullptr, nullptr)==COMMA) {
      eat_token();
      tmr = year_day_range();
      if (!tmr) {
         DRETURN(nullptr);
      }
      ydrl->push_back(std::move(tmr));
   }

   DRETURN(ydrl);
}

/**
 * @brief Parses a year day range
 *
 * year_day_range := year_day[-year_day]
 *
 * @return the range (TMR_Type), an empty handle on a syntax error
 */
static ocs::tom::ElementPtr<ocs::tom::TMR> year_day_range() {
   DENTER(TOP_LAYER);

   ocs::tom::ElementPtr<ocs::tom::TM> t2;

   auto t1 = year_day();
   if (!t1) {
      DRETURN(nullptr);
   }

   if (scan(nullptr, nullptr)==MINUS) {
      eat_token();

      t2 = year_day();
      if (!t2) {
         DRETURN(nullptr);
      }
      if (tm_yday_cmp(*t1, *t2)>0) {
         snprintf(parse_error, sizeof(parse_error), SFNMAX,
                  MSG_ANSWER_FIRSTYESTERDAYINRANGEMUSTBEBEFORESECONDYESTERDAY);
         DRETURN(nullptr);
      }
   }

   auto tmr = ocs::tom::ElementPtr<ocs::tom::TMR>::create();

   auto tmlp = ocs::tom::ListPtr<ocs::tom::TM>::create("tm_list");
   tmlp->push_back(std::move(t1));
   ocs::tom::set<TMR_begin>(tmr, std::move(tmlp));

   if (t2) {
      auto tmlp = ocs::tom::ListPtr<ocs::tom::TM>::create("tm_list");
      tmlp->push_back(std::move(t2));
      ocs::tom::set<TMR_end>(tmr, std::move(tmlp));
   }

   DRETURN(tmr);
}

/**
 * @brief Parses a year day
 *
 * year_day := day.month.year
 *
 * @return the day (TM_Type), an empty handle on a syntax error
 */
static ocs::tom::ElementPtr<ocs::tom::TM> year_day() {
   DENTER(TOP_LAYER);

   int y, m, d;

   if (day(&d)) {
      DRETURN(nullptr);
   }

   if (scan(nullptr, nullptr)!=DOT) {
      snprintf(parse_error, sizeof(parse_error), SFNMAX, MSG_PARSE_MISSINGPOINTAFTERDAY);
      DRETURN(nullptr);
   }
   eat_token();

   if (month(&m)) {
      DRETURN(nullptr);
   }

   if (scan(nullptr, nullptr)!=DOT) {
      snprintf(parse_error, sizeof(parse_error), SFNMAX, MSG_PARSE_MISSINGPOINTAFTERMONTH );
      DRETURN(nullptr);
   }
   eat_token();

   if (year(&y)) {
      DRETURN(nullptr);
   }

   auto tm = ocs::tom::ElementPtr<ocs::tom::TM>::create();
   ocs::tom::set<TM_year>(tm, y - 1900);
   ocs::tom::set<TM_mon>(tm, m);
   ocs::tom::set<TM_mday>(tm, d);

   DRETURN(tm);
}

static int range_number(int min, int max, int *ip, const char *name) {
   DENTER(TOP_LAYER);

   if (scan(nullptr, nullptr)==NUMBER) {
      int this_number = get_number();
      eat_token();

      if (this_number > max || this_number < min) {
         snprintf(parse_error, sizeof(parse_error), MSG_PARSE_WOUTSIDEOFRANGEXYZ_SIIS, get_string(),  min, max, name);
         DRETURN(-1);
      } else {
         if (ip)
            *ip = this_number;
         DRETURN(0);
      }
   }

   snprintf(parse_error, sizeof(parse_error), MSG_PARSE_XISNOTAY_SS , get_string(), name);
   DRETURN(-1);
}


static int month(int *mp) {
   DENTER(TOP_LAYER);

   int m;

   static token_set_t monthv[] = {
      { 1,  "january" },
      { 2,  "february" },
      { 3,  "march" },
      { 4,  "april" },
      { 5,  "may" },
      { 6,  "june" },
      { 7,  "july" },
      { 8,  "august" },
      { 9,  "september" },
      { 10, "october" },
      { 11, "november" },
      { 12, "december" },
      { -1, nullptr }
   };

   if (scan(nullptr, nullptr)==STRING) {
      char *s = get_string();
      if ((m = cheap_scan(s, monthv, 3, "month"))<0) {
         DRETURN(-1);
      }
      eat_token();
   } else {
      if (range_number(1, 12, &m, "month")<0) {
         DRETURN(-1);
      }
   }
   *mp = m - 1;

   DRETURN(0);
}

static int day(int *dp) {
   return range_number(1, 31, dp, "day");
}

static int year(int *yp) {
   return range_number(1970, 2037, yp, "year");
}

/**
 * @brief Parses a list of daytime ranges
 *
 * daytime_range_list := daytime_range[,daytime_range] ...
 *
 * A range that wraps around midnight is split into two.
 *
 * @return the range list (TMR_Type), an empty handle on a syntax error
 */
static ocs::tom::ListPtr<ocs::tom::TMR> daytime_range_list() {
   DENTER(TOP_LAYER);

   auto tmr = daytime_range();
   if (!tmr) {
      DRETURN(nullptr);
   }
   auto dtrl = ocs::tom::ListPtr<ocs::tom::TMR>::create("daytime_range_list");
   auto added = dtrl->push_back(std::move(tmr));
   split_daytime_range(*dtrl, added);

   while (scan(nullptr, nullptr)==COMMA) {
      eat_token();
      tmr = daytime_range();
      if (!tmr) {
         DRETURN(nullptr);
      }
      added = dtrl->push_back(std::move(tmr));
      split_daytime_range(*dtrl, added);
   }

   DRETURN(dtrl);
}

/**
 * @brief Splits a daytime range that wraps around midnight into two
 *
 * The range keeps its begin and ends at 24:00; a new range from 0:00 to the
 * old end is appended to the list.
 *
 * @param dtrl the range list (TMR_Type) holding the range
 * @param tmr the range (TMR_Type)
 */
static void split_daytime_range(const ocs::tom::List<ocs::tom::TMR> dtrl, const ocs::tom::Element<ocs::tom::TMR> tmr) {
   DENTER(TOP_LAYER);

   if (const auto t2 = ocs::tom::get<TMR_end>(tmr).front()) {
      const auto t1 = ocs::tom::get<TMR_begin>(tmr).front();

      if (tm_daytime_cmp(t1, t2)>0) {
         /* split it into two ranges */
         auto new_range = ocs::tom::ElementPtr<ocs::tom::TMR>::create();
         ocs::tom::emplace_back_in<TMR_begin, TM_hour>(new_range, 0);
         ocs::tom::emplace_back_in<TMR_end, TM_hour>(new_range, 24);
         ocs::tom::swap<TMR_end, TMR_end>(tmr, *new_range);
         const auto tmr2 = dtrl.push_back(std::move(new_range));

         const auto begin1 = ocs::tom::get<TMR_begin>(tmr).front();
         const auto end1 = ocs::tom::get<TMR_end>(tmr).front();
         const auto begin2 = ocs::tom::get<TMR_begin>(tmr2).front();
         const auto end2 = ocs::tom::get<TMR_end>(tmr2).front();

         DPRINTF("splitted %d:%d:%d-%d:%d:%d into %d:%d:%d-%d:%d:%d and %d:%d:%d-%d:%d:%d\n",
            ocs::tom::get<TM_hour>(begin1),
            ocs::tom::get<TM_min>(begin1),
            ocs::tom::get<TM_sec>(begin1),

            ocs::tom::get<TM_hour>(end2),
            ocs::tom::get<TM_min>(end2),
            ocs::tom::get<TM_sec>(end2),

            ocs::tom::get<TM_hour>(begin1),
            ocs::tom::get<TM_min>(begin1),
            ocs::tom::get<TM_sec>(begin1),

            ocs::tom::get<TM_hour>(end1),
            ocs::tom::get<TM_min>(end1),
            ocs::tom::get<TM_sec>(end1),

            ocs::tom::get<TM_hour>(begin2),
            ocs::tom::get<TM_min>(begin2),
            ocs::tom::get<TM_sec>(begin2),

            ocs::tom::get<TM_hour>(end2),
            ocs::tom::get<TM_min>(end2),
            ocs::tom::get<TM_sec>(end2));
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Parses a daytime range
 *
 * daytime_range := daytime-daytime
 *
 * @return the range (TMR_Type), an empty handle on a syntax error
 */
static ocs::tom::ElementPtr<ocs::tom::TMR> daytime_range() {
   DENTER(TOP_LAYER);

   auto t1 = daytime();
   if (!t1) {
      DRETURN(nullptr);
   }
   if (scan(nullptr, nullptr)!=MINUS) {
      snprintf(parse_error, sizeof(parse_error), SFNMAX, MSG_PARSE_MISSINGDASHINDAYTIMERANGE);
      DRETURN(nullptr);
   }
   eat_token();
   auto t2 = daytime();
   if (!t2) {
      DRETURN(nullptr);
   }
   if (!tm_daytime_cmp(*t1, *t2)) {
      snprintf(parse_error, sizeof(parse_error), SFNMAX, MSG_PARSE_RANGEBEGISEQUALTOEND);
      DRETURN(nullptr);
   }

   auto tmr = ocs::tom::ElementPtr<ocs::tom::TMR>::create();

   auto tmlp = ocs::tom::ListPtr<ocs::tom::TM>::create("tm_list");
   tmlp->push_back(std::move(t1));
   ocs::tom::set<TMR_begin>(tmr, std::move(tmlp));

   tmlp = ocs::tom::ListPtr<ocs::tom::TM>::create("tm_list");
   tmlp->push_back(std::move(t2));
   ocs::tom::set<TMR_end>(tmr, std::move(tmlp));

   DRETURN(tmr);
}


/**
 * @brief Parses a daytime
 *
 * daytime := hour[:minute][:second]
 *
 * @return the daytime (TM_Type), an empty handle on a syntax error
 */
static ocs::tom::ElementPtr<ocs::tom::TM> daytime() {
   DENTER(TOP_LAYER);

   int h, m = 0, s = 0;

   if (hour(&h)) {
      DRETURN(nullptr);
   }

   if (scan(nullptr, nullptr)!=COLON) {
      goto SUCCESS;
   }
   eat_token();
   if (minute(&m)) {
      DRETURN(nullptr);
   }

   if (scan(nullptr, nullptr)!=COLON) {
      goto SUCCESS;
   }
   eat_token();
   if (seconds(&s)) {
      DRETURN(nullptr);
   }

SUCCESS:
   if (h==24) {
      if (m || s) {
         snprintf(parse_error, sizeof(parse_error), SFNMAX, MSG_PARSE_DAYTIMESBEYOND24HNOTALLOWED);
         DRETURN(nullptr);
      }
   }

   auto tm = ocs::tom::ElementPtr<ocs::tom::TM>::create();
   ocs::tom::set<TM_hour>(tm, h);
   ocs::tom::set<TM_min>(tm, m);
   ocs::tom::set<TM_sec>(tm, s);

   DRETURN(tm);
}

static int hour(int *hp) {
   return range_number(0, 24, hp, MSG_PARSE_HOURSPEC);
}

static int minute(int *mp) {
   return range_number(0, 59, mp, MSG_PARSE_MINUTESPEC);
}

static int seconds(int *sp) {
   return range_number(0, 59, sp, MSG_PARSE_SECONDSSPEC);
}

static int action(int *sp) {
   DENTER(TOP_LAYER);

   int state;
   char *s;
   if (scan(nullptr, nullptr)!=STRING) {
      snprintf(parse_error, sizeof(parse_error), MSG_PARSE_XISNOTASTATESPECIFIER_S, get_string());
      DRETURN(-1);
   }

   s = get_string();
   if ((state = cheap_scan(s, statev, 3, "state specifier"))<0) {
      snprintf(parse_error, sizeof(parse_error), MSG_PARSE_XISNOTASTATESPECIFIER_S, s);
      DRETURN(-1);
   }
   eat_token();
   *sp = state;

   DRETURN(0);
}

/**
 * @brief Parses a week calendar
 *
 * disabled_week_list := disabled_week_entry[<space>disabled_week_entry]
 *
 * @param alpp receives the syntax error
 * @param s the week calendar specification; nullptr or "none" for no calendar
 * @param cal_name the name of the calendar, for the error message
 * @return the parsed calendar (CA_Type), an empty handle for no calendar, std::nullopt on a syntax error
 */
static std::optional<ocs::tom::ListPtr<ocs::tom::CA>>
disabled_week_list(lList **alpp, const char *s, const char *cal_name) {
   DENTER(TOP_LAYER);

   static token_set_t token_set[] = {
      { COLON,        ":" },
      { EQUAL_SIGN,   "=" },
      { MINUS,        "-" },
      { COMMA,        "," },
      { SPACE,        " " },
      { STRING,       nullptr },
      { NUMBER,       nullptr },
      { 0,            nullptr }
   };

   ocs::tom::ListPtr<ocs::tom::CA> cal;

   if (!s || !strcasecmp(s, "none")) {
      DRETURN(std::move(cal));
   }

   scan(s, token_set);

   {
      auto calep = disabled_week_entry();
      if (!calep) {
         goto ERROR;
      }

      cal = ocs::tom::ListPtr<ocs::tom::CA>::create("week list");
      cal->push_back(std::move(calep));
   }

   /* if we have a space, there is at least one more calender spec. */
   while (scan(nullptr, nullptr) == SPACE) {
      eat_token();

      /* there can be multiple spaces between calender def. eat them all */
      while (scan(nullptr, nullptr) ==SPACE) {
         eat_token();
      }

      auto calep = disabled_week_entry();
      if (!calep) {
         goto ERROR;
      }
      cal->push_back(std::move(calep));
   }

   /* complain about still unused tokens */
   if (scan(nullptr, nullptr)!=NO_TOKEN) {
      snprintf(parse_error, sizeof(parse_error), SFNMAX, MSG_PARSE_UNRECOGNIZEDTOKENATEND);
      goto ERROR;
   }

   DRETURN(std::move(cal));

ERROR:
   cal.reset();
   snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_PARSE_ERRORINDISABLEDWEEKOFCALENDAR_SS, cal_name, save_error());
   answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
   DRETURN(std::nullopt);
}

/**
 * @brief Parses one entry of a week calendar
 *
 * disabled_week_entry := week_day_range_list[=daytime_range_list][=state]
 *
 * @return the entry (CA_Type), an empty handle on a syntax error
 */
static ocs::tom::ElementPtr<ocs::tom::CA> disabled_week_entry() {
   DENTER(TOP_LAYER);

   /* default values */
   ocs::tom::ListPtr<ocs::tom::TMR> wdrl;
   ocs::tom::ListPtr<ocs::tom::TMR> dtrl;
   int state = QI_DO_DISABLE;

   if (scan(nullptr, nullptr)==STRING &&
       cheap_scan(get_string(), statev, 3, "state specifier")<0) {

      wdrl = week_day_range_list();
      if (!wdrl) {
         goto ERROR;
      }

      if (scan(nullptr, nullptr)!=EQUAL_SIGN) {
         goto SUCCESS;
      }
      eat_token();
   }

   if (scan(nullptr, nullptr)==NUMBER) {
      dtrl = daytime_range_list();
      if (!dtrl) {
         goto ERROR;
      }
      normalize_range_list(*dtrl, tm_daytime_cmp);
      if (scan(nullptr, nullptr)!=EQUAL_SIGN) {
         goto SUCCESS;
      }
      eat_token();
   }

   if (scan(nullptr, nullptr)==STRING) {
      state = 0;
      if (action(&state)) {
         goto ERROR;
      }
   } else {
      snprintf(parse_error, sizeof(parse_error), SFNMAX, MSG_ANSWER_GOTEQUALWITHOUTDAYTIMERANGEORSTATE);
      goto ERROR;
   }

SUCCESS:
   {
      auto cal = ocs::tom::ElementPtr<ocs::tom::CA>::create();

      if (!wdrl) {
         wdrl = full_weekday_range();
      }

      ocs::tom::set<CA_wday_range_list>(cal, std::move(wdrl));

      if (!dtrl) {
         dtrl = full_daytime_range();
      }
      ocs::tom::set<CA_daytime_range_list>(cal, std::move(dtrl));
      ocs::tom::set<CA_state>(cal, state);
      DRETURN(cal);
   }

ERROR:
   DRETURN(nullptr);
}

/**
 * @brief Parses a list of week day ranges
 *
 * week_day_range_list := week_day_range[,week_day_range]
 *
 * A range that wraps around the end of the week is split into two, touching
 * and overlapping ranges are joined, and ranges are extended over the end of
 * the week.
 *
 * @return the range list (TMR_Type), an empty handle on a syntax error
 */
static ocs::tom::ListPtr<ocs::tom::TMR> week_day_range_list() {
   DENTER(TOP_LAYER);

   auto tmr = week_day_range();
   if (!tmr) {
      DRETURN(nullptr);
   }

   auto wdrl = ocs::tom::ListPtr<ocs::tom::TMR>::create("week_day_range_list");
   auto added = wdrl->push_back(std::move(tmr));
   split_wday_range(*wdrl, added);



   while (scan(nullptr, nullptr)==COMMA) {
      eat_token();
      tmr = week_day_range();
      if (!tmr) {
         DRETURN(nullptr);
      }
      added = wdrl->push_back(std::move(tmr));
      split_wday_range(*wdrl, added);
   }

   /* prepare the structures for later use */
   join_wday_range(*wdrl);
   extend_wday_range(*wdrl);

   DRETURN(wdrl);
}

/**
 * @brief Join overlapping ranges
 *
 * A user can define overlapping ranges in the week calendar. This function
 * will join them into one definition.
 *
 * @param week_day week calendar def. list (TMR_Type)
 *
 * @note MT-NOTE: join_wday_range() is MT safe
 */
static void join_wday_range(const ocs::tom::List<ocs::tom::TMR> week_day) {
   DENTER(TOP_LAYER);

   auto next1 = week_day.front();

   /* join ranges, which overlap or touch */
   while (next1) {
      const auto day_range1 = next1;
      uint32_t begin1;
      uint32_t end1;

      next1 = next1.next();

      const auto beginList1 = ocs::tom::get<TMR_begin>(day_range1);
      const auto endList1 = ocs::tom::get<TMR_end>(day_range1);

      begin1 = ocs::tom::get<TM_wday>(beginList1.front());
      if (endList1 != nullptr) {
         end1 = ocs::tom::get<TM_wday>(endList1.front());
      }
      else {
         end1 = begin1;
      }
      auto next2 = week_day.front();
      while (next2) {
         const auto day_range2 = next2;
         uint32_t begin2;
         uint32_t end2;

         next2 = next2.next();

         if (day_range2 == day_range1) {
            continue;
         }

         const auto beginList2 = ocs::tom::get<TMR_begin>(day_range2);
         const auto endList2 = ocs::tom::get<TMR_end>(day_range2);

         begin2 = ocs::tom::get<TM_wday>(beginList2.front());
         if (endList2 != nullptr) {
            end2 = ocs::tom::get<TM_wday>(endList2.front());
         }
         else {
            end2 = begin2;
         }

         if (begin2 <= (end1 +1) && (begin1 <= begin2)) {
            if (end1 < end2) {
               if (endList1) {
                  ocs::tom::set<TM_wday>(endList1.front(), end2);
               }
               else {
                  ocs::tom::set<TMR_end>(day_range1, endList2.clone(""));
               }
            }

            if (next1 == day_range2) {
               next1 = next2;
            }
            week_day.erase(day_range2);
         }

      }
   }

   DRETURN_VOID;
}

/**
 * @brief Extend ranges over the week border
 *
 * extend the range over the one week border. The week ends on Sunday, but the
 *  range can go till Monday or longer. But the beginning till the end will never
 *  contain more than 7 days (a week).  This is needed!!
 *
 * @param week_day calendar list for a week (TMR_Type)
 *
 * @note MT-NOTE: extend_wday_range() is MT safe
 */
static void extend_wday_range(const ocs::tom::List<ocs::tom::TMR> week_day) {
   DENTER(TOP_LAYER);

   auto next1 = week_day.front();
   while (next1) {
      const auto day_range1 = next1;
      uint32_t begin1;
      uint32_t end1;

      next1 = next1.next();

      const auto beginList1 = ocs::tom::get<TMR_begin>(day_range1);
      auto endList1 = ocs::tom::get<TMR_end>(day_range1);

      begin1 = ocs::tom::get<TM_wday>(beginList1.front());
      if (endList1 != nullptr) {
         end1 = ocs::tom::get<TM_wday>(endList1.front());
      }
      else {
         end1 = begin1;
      }

      auto next2 = week_day.front();
      while (next2) {
         const auto day_range2 = next2;
         uint32_t begin2;
         uint32_t end2;

         next2 = next2.next();

         if (day_range2 == day_range1) {
            continue;
         }

         const auto beginList2 = ocs::tom::get<TMR_begin>(day_range2);
         const auto endList2 = ocs::tom::get<TMR_end>(day_range2);

         begin2 = ocs::tom::get<TM_wday>(beginList2.front());
         if (endList2 != nullptr) {
            end2 = ocs::tom::get<TM_wday>(endList2.front());
         }
         else {
            end2 = begin2;
         }

         if ((end1 == 6) && (begin2 == 0)) {
            int tempEnd = end1 + 1 + end2;
            if (endList1 == nullptr) {
               auto copy = beginList1.clone("");
               endList1 = *copy;
               ocs::tom::set<TMR_end>(day_range1, std::move(copy));
            }
            ocs::tom::set<TM_wday>(endList1.front(), tempEnd);
         }
      }
   }

   DRETURN_VOID;
}


/**
 * @brief Splits a week day range that wraps around the end of the week into two
 *
 * The range keeps its begin and ends on saturday; a new range from sunday to
 * the old end is appended to the list.
 *
 * @param wdrl the range list (TMR_Type) holding the range
 * @param tmr the range (TMR_Type)
 */
static void split_wday_range(const ocs::tom::List<ocs::tom::TMR> wdrl, const ocs::tom::Element<ocs::tom::TMR> tmr) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstElement<ocs::tom::TM> t2, t1; /* *t3, *t4, */

   if ((t2=ocs::tom::get<TMR_end>(tmr).front())) {
      t1=ocs::tom::get<TMR_begin>(tmr).front();

      if (tm_wday_cmp(t1, t2) > 0) {
         /* split it into two ranges */
         auto tmr2 = ocs::tom::ElementPtr<ocs::tom::TMR>::create();
         ocs::tom::emplace_back_in<TMR_begin, TM_wday>(tmr2, 0);
         ocs::tom::emplace_back_in<TMR_end, TM_wday>(tmr2, 6);
         ocs::tom::swap<TMR_end, TMR_end>(tmr, *tmr2);
         wdrl.push_back(std::move(tmr2));

         t1=ocs::tom::get<TMR_begin>(tmr).front();
         t2=ocs::tom::get<TMR_end>(tmr).front();
/*          t3=lFirst(lGetList(tmr2, TMR_begin)); */
/*          t4=lFirst(lGetList(tmr2, TMR_end)); */

#if 0
      DPRINTF(("splitted wday %d-%d into %d-%d and %d-%d\n",
         lGetUlong(t1, TM_wday),
         lGetUlong(t4, TM_wday),

         lGetUlong(t1, TM_wday),
         lGetUlong(t2, TM_wday),

         lGetUlong(t3, TM_wday),
         lGetUlong(t4, TM_wday)));
#endif
      }
   }

   DRETURN_VOID;
}

/**
 * @brief Parses a week day range
 *
 * week_day_range := week_day[-week_day]
 *
 * @return the range (TMR_Type), an empty handle on a syntax error
 */
static ocs::tom::ElementPtr<ocs::tom::TMR> week_day_range() {
   DENTER(TOP_LAYER);

   ocs::tom::ElementPtr<ocs::tom::TM> t2;

   auto t1 = week_day();
   if (!t1) {
      DRETURN(nullptr);
   }

   if (scan(nullptr, nullptr)==MINUS) {
      eat_token();

      t2 = week_day();
      if (!t2) {
         DRETURN(nullptr);
      }

      if (tm_wday_cmp(*t1, *t2)==0) {
         snprintf(parse_error, sizeof(parse_error), SFNMAX, MSG_PARSE_FOUNDUSELESSWEEKDAYRANGE);
         DRETURN(nullptr);
      }
   }


   auto tmr = ocs::tom::ElementPtr<ocs::tom::TMR>::create();

   auto tmlp = ocs::tom::ListPtr<ocs::tom::TM>::create("tm_list");
   tmlp->push_back(std::move(t1));
   ocs::tom::set<TMR_begin>(tmr, std::move(tmlp));

   if (t2) {
      tmlp = ocs::tom::ListPtr<ocs::tom::TM>::create("tm_list");
      tmlp->push_back(std::move(t2));
      ocs::tom::set<TMR_end>(tmr, std::move(tmlp));
   }

   DRETURN(tmr);
}

/**
 * @brief Parses a week day
 *
 * @return the week day (TM_Type), an empty handle on a syntax error
 */
static ocs::tom::ElementPtr<ocs::tom::TM> week_day() {
   DENTER(TOP_LAYER);

   int wday;
   char *s;

   static token_set_t weekdayv[] = {
      { 0, "sunday" },
      { 1, "monday" },
      { 2, "tuesday" },
      { 3, "wednesday" },
      { 4, "thursday" },
      { 5, "friday" },
      { 6, "saturday" },
      { -1, nullptr }
   };

   if (scan(nullptr, nullptr)!=STRING) {
      snprintf(parse_error, sizeof(parse_error), SFNMAX, MSG_PARSE_EXPECTEDSTRINGFORWEEKDAY);
      DRETURN(nullptr);
   }
   s = get_string();
   if ((wday = cheap_scan(s, weekdayv, 3, "weekday"))<0) {
      snprintf(parse_error, sizeof(parse_error), MSG_PARSE_XISNOTAWEEKDAY_S, s);
      DRETURN(nullptr);
   }
   eat_token();

   auto tm = ocs::tom::ElementPtr<ocs::tom::TM>::create();
   ocs::tom::set<TM_wday>(tm, wday);

   DRETURN(tm);
}

static char *get_string() {
   return store;
}

static int get_number() {
   return number;
}
static char *save_error() {
   strcpy(old_error, parse_error);
   return old_error;
}

/* ------------------------------------------------------------

   eat_token() informs parsing system that the current token
   is proceed

 */
static void eat_token() {
/*    DENTER(TOP_LAYER); */
/*    DPRINTF("token \"%s\"\n", store); */
   token_is_valid = 0;
/*    DRETURN_VOID; */
}

static int scan(const char *s, token_set_t token_set[]) {
   DENTER(CULL_LAYER);

   static const char *t = nullptr;
   static token_set_t *ts = nullptr;
   static int token;

   int i, j, k, len = 0;
   int found;

   if (s) {                     /* initialize scan() with a new string to parse */
      t = s;
      token_is_valid = 0;
      old_error[0] = parse_error[0] = '\0';
   }
   if (token_set) {
      ts = token_set;
   }

   if (token_is_valid) {        /* no need for a new token to parse */
      DRETURN(token);
   }

   if (!*t) {
      token_is_valid = 1;
      token = NO_TOKEN;
      DRETURN(token);
   }

   /* try all possible tokens */
   for (i = 0; ts[i].token; i++) {
      switch (ts[i].token) {

         case NUMBER: {
                     int old_number = 0;
                     /* parse number 0-9* and store it */
                     for (j=0; t[j] && isdigit((int) t[j]); j++) {
                        number =  old_number * 10 + (t[j]-'0');
                        if (number<old_number) {
                           ERROR(SFNMAX, MSG_PARSE_OVERFLOWERRORWHILEPARSING);
                           token = ERR_TOKEN;
                           token_is_valid = 1;
                           DRETURN(token);
                        } else
                           old_number = number;
                     }
                     if ((found = (j!=0))) {
                        len = j;
                        strncpy(store, t, len);
                        store[len] = '\0';
                     }
                  }
            break;

         case STRING:   /* strings and shortcut strings */
                  /* parse string a-z,A-Z[a-z,A-Z,0-9]* and store it */
                  j = k = 0;
                  if (isalpha((int)t[j])) {
                     store[k++] = t[j];
                     for (j++; t[j] && isalnum((int)t[j]); j++) {
                        store[k++] = t[j];
                     }
                     store[k] = '\0';
                  }

                  len = j;
                  if (ts[i].token == STRING) {
                     found = (j!=0);
                     break;
                  }

                  found = !strcasecmp(ts[i].text, store);
            break;
         default:
               len = strlen(ts[i].text);
               found = !strncasecmp(ts[i].text, t, len);
               strncpy(store, t, len);
               store[len] = '\0';
         break;
      }

      if (found) {
         t += len;
         token_is_valid = 1;
         DRETURN((token = ts[i].token));
      }
   }

   /* nothing found */
   token_is_valid = 1;
   token = ERR_TOKEN;
   DRETURN(token);
}

static int cheap_scan(char *s, token_set_t tokenv[], int n, const char *name) {
   DENTER(TOP_LAYER);

   int i;
   int len;
   int match_all_chars = 0;

   if ((len=strlen(s)) < n) {
      match_all_chars = 1;
   }

   for (i=0; tokenv[i].text; i++) {
      if (match_all_chars ?
          !strcasecmp(tokenv[i].text, s):
          !strncasecmp(tokenv[i].text, s, len)) {

/*             DPRINTF("recognized \"%s\" == \"%s\" with %d\n", tokenv[i].text, s, tokenv[i].token);  */

         DRETURN(tokenv[i].token);
      }
   }

   DRETURN(tokenv[i].token);
}

/**
 * @brief Orders two times by their year day
 *
 * @param t1 the first time (TM_Type)
 * @param t2 the second time (TM_Type)
 * @return negative, zero or positive as `t1` is before, on or after the day of `t2`
 */
static int tm_yday_cmp(const ocs::tom::ConstElement<ocs::tom::TM> t1, const ocs::tom::ConstElement<ocs::tom::TM> t2) {
   DENTER(TOP_LAYER);

   int t;

/*   DPRINTF(("tm_yday_cmp(y%d:m%d:d%d, y%d:m%d:d%d)\n",
      lGetUlong(t1, TM_year),
      lGetUlong(t1, TM_mon),
      lGetUlong(t1, TM_mday),
      lGetUlong(t2, TM_year),
      lGetUlong(t2, TM_mon),
      lGetUlong(t2, TM_mday))); */

   if ((t=(ocs::tom::get<TM_year>(t1) - ocs::tom::get<TM_year>(t2)))) {
      DRETURN(t);
   }
   if ((t=(ocs::tom::get<TM_mon>(t1) - ocs::tom::get<TM_mon>(t2)))) {
      DRETURN(t);
   }
   DRETURN(ocs::tom::get<TM_mday>(t1) - ocs::tom::get<TM_mday>(t2));
}

/**
 * @brief Orders two times by their week day
 *
 * @param t1 the first time (TM_Type)
 * @param t2 the second time (TM_Type)
 * @return negative, zero or positive as `t1` is before, on or after the week day of `t2`
 */
static int tm_wday_cmp(const ocs::tom::ConstElement<ocs::tom::TM> t1, const ocs::tom::ConstElement<ocs::tom::TM> t2) {
   DENTER(TOP_LAYER);
   DRETURN(ocs::tom::get<TM_wday>(t1) - ocs::tom::get<TM_wday>(t2));
}

/**
 * @brief Orders two times by their daytime
 *
 * @param t1 the first time (TM_Type)
 * @param t2 the second time (TM_Type)
 * @return negative, zero or positive as `t1` is before, at or after the daytime of `t2`
 */
static int tm_daytime_cmp(const ocs::tom::ConstElement<ocs::tom::TM> t1,
                          const ocs::tom::ConstElement<ocs::tom::TM> t2) {
   DENTER(TOP_LAYER);

   int t;

   if ((t=(ocs::tom::get<TM_hour>(t1) - ocs::tom::get<TM_hour>(t2)))) {
      DRETURN(t);
   }
   if ((t=(ocs::tom::get<TM_min>(t1) - ocs::tom::get<TM_min>(t2)))) {
      DRETURN(t);
   }
   DRETURN(ocs::tom::get<TM_sec>(t1) - ocs::tom::get<TM_sec>(t2));
}

/**
 * @brief Copy a `struct tm` into a CULL element
 *
 * The calendar computation works on CULL elements so intermediate results can
 * be put into a list; this is the conversion in.
 *
 * @param[out] tm_ep the element to fill (TM_Type)
 * @param tm_now the time to copy
 */
void cullify_tm(const ocs::tom::Element<ocs::tom::TM> tm_ep, const struct tm *tm_now) {
   DENTER(TOP_LAYER);

   ocs::tom::set<TM_mday>(tm_ep,  tm_now->tm_mday);
   ocs::tom::set<TM_mon>(tm_ep,   tm_now->tm_mon);
   ocs::tom::set<TM_year>(tm_ep,  tm_now->tm_year);
   ocs::tom::set<TM_sec>(tm_ep,   tm_now->tm_sec);
   ocs::tom::set<TM_min>(tm_ep,   tm_now->tm_min);
   ocs::tom::set<TM_hour>(tm_ep,  tm_now->tm_hour);
   ocs::tom::set<TM_wday>(tm_ep,  tm_now->tm_wday);
   ocs::tom::set<TM_yday>(tm_ep,  tm_now->tm_yday);
   ocs::tom::set<TM_isdst>(tm_ep, tm_now->tm_isdst);

   DRETURN_VOID;
}

/**
 * @brief Copy a CULL element back into a `struct tm`
 *
 * @param tm_ep the element to read (TM_Type)
 * @param[out] tm_now the structure to fill
 */
void uncullify_tm(const ocs::tom::ConstElement<ocs::tom::TM> tm_ep, struct tm *tm_now) {
   DENTER(TOP_LAYER);

   tm_now->tm_mday =  ocs::tom::get<TM_mday>(tm_ep);
   tm_now->tm_mon  =  ocs::tom::get<TM_mon>(tm_ep);
   tm_now->tm_year =  ocs::tom::get<TM_year>(tm_ep);
   tm_now->tm_sec  =  ocs::tom::get<TM_sec>(tm_ep);
   tm_now->tm_min  =  ocs::tom::get<TM_min>(tm_ep);
   tm_now->tm_hour =  ocs::tom::get<TM_hour>(tm_ep);
   tm_now->tm_wday =  ocs::tom::get<TM_wday>(tm_ep);
   tm_now->tm_yday =  ocs::tom::get<TM_yday>(tm_ep);
   tm_now->tm_isdst = ocs::tom::get<TM_isdst>(tm_ep);

   DRETURN_VOID;
}

/**
 * @brief Parse a calendar's `year` specification
 *
 * @param[in,out] cal the calendar whose parsed form is stored
 * @param[out] answer_list receives the syntax error
 * @return true when the specification was understood
 */
bool calendar_parse_year(const ocs::tom::Element<ocs::tom::CAL> cal, lList **answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   auto yc = disabled_year_list(answer_list, ocs::tom::get<CAL_year_calendar>(cal), ocs::tom::get<CAL_name>(cal));
   if (!yc) {
      ret = false;
   } else {
      ocs::tom::set<CAL_parsed_year_calendar>(cal, std::move(*yc));
   }

   DRETURN(ret);
}

/**
 * @brief calendar_parse_year() for an untyped caller
 *
 * @param[in,out] cal the calendar (CAL_Type), checked
 * @param[out] answer_list receives the syntax error
 * @return as calendar_parse_year()
 */
bool calendar_parse_year(lListElem *cal, lList **answer_list) {
   return calendar_parse_year(ocs::tom::Element<ocs::tom::CAL>(cal), answer_list);
}

/**
 * @brief Parse a calendar's `week` specification
 *
 * @param[in,out] cal the calendar whose parsed form is stored
 * @param[out] answer_list receives the syntax error
 * @return true when the specification was understood
 */
bool calendar_parse_week(const ocs::tom::Element<ocs::tom::CAL> cal, lList **answer_list) {
   DENTER(TOP_LAYER);

   bool ret = true;

   auto wc = disabled_week_list(answer_list, ocs::tom::get<CAL_week_calendar>(cal), ocs::tom::get<CAL_name>(cal));
   if (!wc) {
      ret = false;
   } else {
      ocs::tom::set<CAL_parsed_week_calendar>(cal, std::move(*wc));
   }
   DRETURN(ret);
}

/**
 * @brief calendar_parse_week() for an untyped caller
 *
 * @param[in,out] cal the calendar (CAL_Type), checked
 * @param[out] answer_list receives the syntax error
 * @return as calendar_parse_week()
 */
bool calendar_parse_week(lListElem *cal, lList **answer_list) {
   return calendar_parse_week(ocs::tom::Element<ocs::tom::CAL>(cal), answer_list);
}

/**
 * @brief Generates the TODO orders
 *
 * It calles the routins to compute the current state and next change. The current
 * state is than changed into todo orders.
 *
 * @param cep calendar (CAL_Type)
 * @param now the time to look at
 *
 * @return what to do (the current state) and the next state change
 *
 * @note MT-NOTE: calendar_get_current_state_and_end() is MT safe
 */
static std::pair<uint32_t, time_t>
calendar_get_current_state_and_end(const ocs::tom::ConstElement<ocs::tom::CAL> cep, const time_t now) {
   DENTER(TOP_LAYER);

   ocs::tom::ConstList<ocs::tom::CA> year_list = nullptr;
   ocs::tom::ConstList<ocs::tom::CA> week_list = nullptr;

   DPRINTF("cal: %s\n", ocs::tom::get<CAL_name>(cep));

   if (cep != nullptr) {
      year_list = ocs::tom::get<CAL_parsed_year_calendar>(cep);
      week_list = ocs::tom::get<CAL_parsed_week_calendar>(cep);
   }

   const auto [state, then] = state_at(now, year_list, week_list);

   uint32_t new_state;
   switch (state) {
   case QI_DO_DISABLE:
      new_state = QI_DO_CAL_DISABLE;
      break;
   case QI_DO_SUSPEND:
      new_state = QI_DO_CAL_SUSPEND;
      break;
   default:
      new_state = QI_DO_NOTHING;
      break;
   }

   DRETURN(std::make_pair(new_state, then));
}

/**
 * @brief Is a calendar used by any cluster queue?
 *
 * Callers use it to refuse deleting a calendar a queue still refers to.
 *
 * @param calendar the calendar to look for
 * @param[out] answer_list receives the name of every queue using it
 * @param master_cqueue_list the cluster queues to search
 * @return true when at least one queue refers to it
 */
bool calendar_is_referenced(const ocs::tom::ConstElement<ocs::tom::CAL> calendar, lList **answer_list,
                            const ocs::tom::ConstList<ocs::tom::CQ> master_cqueue_list) {
   DENTER(TOP_LAYER);

   bool ret = false;

   /*
    * fix for bug 6422335
    * check the cq configuration for calendar references instead of qinstances
    */
   const char *calendar_name = ocs::tom::get<CAL_name>(calendar);
   if (calendar_name != nullptr) {
      for (const auto cqueue : master_cqueue_list) {
         for (const auto cal : ocs::tom::get<CQ_calendar>(cqueue)) {
            const char *qcal_name = ocs::tom::get<ASTR_value>(cal);
            if (qcal_name && !strcmp(qcal_name, calendar_name)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                       ANSWER_QUALITY_INFO,
                                       MSG_CALENDAR_REFINQUEUE_SS,
                                       calendar_name,
                                       ocs::tom::get<CQ_name>(cqueue));
               ret = true;
               break;
            }
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief calendar_is_referenced() for an untyped caller
 *
 * @param calendar the calendar (CAL_Type), checked
 * @param[out] answer_list receives the name of every queue using it
 * @param master_cqueue_list the cluster queues (CQ_Type), checked
 * @return as calendar_is_referenced()
 */
bool calendar_is_referenced(const lListElem *calendar, lList **answer_list, const lList *master_cqueue_list) {
   return calendar_is_referenced(ocs::tom::ConstElement<ocs::tom::CAL>(calendar), answer_list,
                                 ocs::tom::ConstList<ocs::tom::CQ>(master_cqueue_list));
}

/**
 * @brief A calendar object template, as `qconf -scal template` prints it
 *
 * @param cal_name the name to give the template, nullptr for "template"
 * @return the new element
 */
ocs::tom::ElementPtr<ocs::tom::CAL> sge_generic_cal(const char *cal_name) {
   DENTER(TOP_LAYER);


   auto calp = ocs::tom::ElementPtr<ocs::tom::CAL>::create();

   ocs::tom::set<CAL_name>(calp, cal_name?cal_name:"template");

   DRETURN(calp);
}

/**
 * @brief Check if calender is open in given time
 *
 * Returns the state (only open or closed) of a calendar in a given time
 * frame
 *
 * @param cep calendar object (CAL_Type)
 * @param start_time time frame start
 * @param duration time frame duration
 *
 * @return true if open false if closed
 *
 * @note MT-NOTE: calendar_open_in_time_frame() is MT safe
 */
bool calendar_open_in_time_frame(const ocs::tom::ConstElement<ocs::tom::CAL> cep, const uint64_t start_time,
                                 const uint64_t duration) {
   DENTER(TOP_LAYER);

   bool ret = true;
   int state;
   ocs::tom::ConstList<ocs::tom::CA> year_list = nullptr;
   ocs::tom::ConstList<ocs::tom::CA> week_list = nullptr;
   time_t next_change;
   time_t start = sge_gmt64_to_time_t(start_time);
   time_t end = sge_gmt64_to_time_t(duration_add_offset(start_time, duration));

   if (cep != nullptr) {
      year_list = ocs::tom::get<CAL_parsed_year_calendar>(cep);
      week_list = ocs::tom::get<CAL_parsed_week_calendar>(cep);
   }

   std::tie(state, next_change) = state_at(start, year_list, week_list);
   while (state == QI_DO_ENABLE && next_change != 0 && next_change <= end) {
      start = next_change;
      std::tie(state, next_change) = state_at(start, year_list, week_list);
   }

   if (state != QI_DO_ENABLE) {
      ret = false;
   }

   DRETURN(ret);
}

/**
 * @brief calendar_open_in_time_frame() for an untyped caller
 *
 * @param cep calendar object (CAL_Type), checked; may be nullptr
 * @param start_time time frame start
 * @param duration time frame duration
 *
 * @return as calendar_open_in_time_frame()
 */
bool calendar_open_in_time_frame(const lListElem *cep, const uint64_t start_time, const uint64_t duration) {
   return calendar_open_in_time_frame(ocs::tom::ConstElement<ocs::tom::CAL>(cep), start_time, duration);
}
