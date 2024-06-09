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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <chrono>
#include <sys/times.h>

#include <sys/time.h>

#include "uti/sge_dstring.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"
#include "uti/sge_log.h"

#define NESTLEVEL 5

/* MT-NOTE: stopwatch profiling used in qmaster, qstat and qmon only */
/* MT-NOTE: stopwatch is phased out and will be replaced by libs/uti/sge_profiling */
static struct tms begin[NESTLEVEL];
static struct tms end[NESTLEVEL];

static time_t wtot[NESTLEVEL];
static time_t wbegin[NESTLEVEL];
static time_t wprev[NESTLEVEL];
static time_t wdiff[NESTLEVEL];

static int clock_tick;
static int time_log_interval[NESTLEVEL] = {-1, -1, -1, -1, -1};

#ifdef TIMES_RETURNS_ZERO
static time_t inittime;
#endif

static void sge_stopwatch_stop(int i);

/* MT-NOTE: sge_stopwatch_stop() is not MT safe due to access to global variables */
static void sge_stopwatch_stop(int i) {
   time_t wend;

   if (i < 0 || i >= NESTLEVEL) {
      return;
   }
   if (time_log_interval[i] == -1) {
      return;
   }
   wend = times(&end[i]);

#ifdef TIMES_RETURNS_ZERO
   /* times() returns 0 on these machines */
   wend = (sge_get_gmt() - inittime) * clock_tick;
#endif

   end[i].tms_utime = end[i].tms_utime - begin[i].tms_utime;
   end[i].tms_stime = end[i].tms_stime - begin[i].tms_stime;
   end[i].tms_cutime = end[i].tms_cutime - begin[i].tms_cutime;
   end[i].tms_cstime = end[i].tms_cstime - begin[i].tms_cstime;

   wtot[i] = wend - wbegin[i];
   wdiff[i] = wend - wprev[i];
   wprev[i] = wend;
}

/****** uti/time/sge_get_gmt() ************************************************
*  NAME
*     sge_get_gmt() -- Return current time 
*
*  SYNOPSIS
*     u_long32 sge_get_gmt() 
*
*  FUNCTION
*     Return current time 
*
*  NOTES
*     MT-NOTE: sge_get_gmt() is MT safe
*
*  RESULT
*     u_long32 - 32 bit time value
******************************************************************************/
u_long32 sge_get_gmt() {
   struct timeval now{};

#ifdef SOLARIS
   gettimeofday(&now, nullptr);
#else
   struct timezone tzp{};
   gettimeofday(&now, &tzp);
#endif

   return (u_long32) now.tv_sec;
}

u_long64 sge_get_gmt64() {
   const auto now = std::chrono::system_clock::now();
   const auto epoch = now.time_since_epoch();
   const auto us = duration_cast<std::chrono::microseconds>(epoch);
   return us.count();
}

u_long32 sge_gmt64_to_gmt32(u_long64 timestamp) {
   return timestamp / 1000000;
}

double sge_gmt64_to_gmt32_double(u_long64 timestamp) {
   return timestamp / 1000000.0;
}

u_long64 sge_gmt32_to_gmt64(u_long32 timestamp) {
   return timestamp * 1000000;
}


const char *append_time(u_long64 timestamp, dstring *dstr, bool is_xml) {
   DSTRING_STATIC(local_dstr, 100);
   return sge_dstring_append(dstr, sge_ctime64(timestamp, &local_dstr, is_xml, true));
}

/****** uti/time/append_time() **************************************************
*  NAME
*     append_time() -- Convert time value into string 
*
*  SYNOPSIS
*     const char* append_time(time_t i, dstring *buffer) 
*
*  FUNCTION
*     Convert time value into string 
*
*  INPUTS
*     time_t i - time value 
*     dstring *buffer - dstring
*     bool is_xml - write in XML dateTime format?
*
*  RESULT
*     const char* - time string (current time if 'i' was 0) 
*     dstring *buffer - buffer provided by caller
*
*  NOTES
*     MT-NOTE: append_time() is MT safe if localtime_r() can be used
*
*     SHOULD BE REPLACED BY: sge_dstring_append_time()
*
******************************************************************************/
const char *append_time(time_t i, dstring *buffer, bool is_xml) {
   const char *ret;
   struct tm *tm;

#ifdef HAS_LOCALTIME_R
   struct tm tm_buffer{};

   tm = (struct tm *) localtime_r(&i, &tm_buffer);
#else
   tm = localtime(&i);
#endif

   if (is_xml) {
      ret = sge_dstring_sprintf_append(buffer, "%04d-%02d-%02dT%02d:%02d:%02d",
                                 1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
                                 tm->tm_hour, tm->tm_min, tm->tm_sec);
   } else {
      ret = sge_dstring_sprintf_append(buffer, "%02d/%02d/%04d %02d:%02d:%02d",
                                 tm->tm_mon + 1, tm->tm_mday, 1900 + tm->tm_year,
                                 tm->tm_hour, tm->tm_min, tm->tm_sec);
   }

   return ret;
}

/****** uti/time/sge_ctime() **************************************************
*  NAME
*     sge_ctime() -- Convert time value into string 
*
*  SYNOPSIS
*     const char* sge_ctime(time_t i, dstring *buffer) 
*
*  FUNCTION
*     Convert time value into string 
*
*  INPUTS
*     time_t i - 0 or time value 
*
*  RESULT
*     const char* - time string (current time if 'i' was 0) 
*     dstring *buffer - buffer provided by caller
*
*  NOTES
*     MT-NOTE: sge_at_time() is MT safe if localtime_r() can be used
*
*  SEE ALSO
*     uti/time/sge_ctime32()
******************************************************************************/
const char *sge_ctime(time_t i, dstring *buffer) {
#ifdef HAS_LOCALTIME_R
   struct tm tm_buffer {};
#endif
   struct tm *tm;

   if (!i)
      i = (time_t) sge_get_gmt();
#ifndef HAS_LOCALTIME_R
   tm = localtime(&i);
#else
   tm = (struct tm *) localtime_r(&i, &tm_buffer);
#endif
   sge_dstring_sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d",
                       tm->tm_mon + 1, tm->tm_mday, 1900 + tm->tm_year,
                       tm->tm_hour, tm->tm_min, tm->tm_sec);

   return sge_dstring_get_string(buffer);
}

/* TODO: should be replaced by sge_dstring_append_time() */
const char *sge_ctimeXML(time_t i, dstring *buffer) {
#ifdef HAS_LOCALTIME_R
   struct tm tm_buffer;
#endif
   struct tm *tm;

   if (!i)
      i = (time_t) sge_get_gmt();
#ifndef HAS_LOCALTIME_R
   tm = localtime(&i);
#else
   tm = (struct tm *) localtime_r(&i, &tm_buffer);
#endif
   sge_dstring_sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d",
                       1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday,
                       tm->tm_hour, tm->tm_min, tm->tm_sec);

   return sge_dstring_get_string(buffer);
}


/****** uti/time/sge_ctime32() ************************************************
*  NAME
*     sge_ctime32() -- Convert time value into string (64 bit time_t) 
*
*  SYNOPSIS
*     const char* sge_ctime32(u_long32 *i, dstring *buffer) 
*
*  FUNCTION
*     Convert time value into string. This function is needed for 
*     systems with a 64 bit time_t because the ctime function would
*     otherwise try to interpret the u_long32 value as 
*     a 64 bit value => $&&%$?$%!
*
*  INPUTS
*     u_long32 *i - 0 or time value 
*     dstring *buffer - buffer provided by caller
*
*  RESULT
*     const char* - time string (current time if 'i' was 0)
* 
*  NOTE
*     MT-NOTE: if ctime_r() is not available sge_ctime32() is not MT safe
*
*  SEE ALSO
*     uti/time/sge_ctime()
******************************************************************************/
const char *sge_ctime32(u_long32 *i, dstring *buffer) {
   const char *s;
#ifdef HAS_CTIME_R
   char str[128];
#endif
#if SOLARIS64
   volatile
#endif
   time_t temp = (time_t) *i;

#ifndef HAS_CTIME_R
   /* if ctime_r() does not exist a mutex must be used to guard *all* ctime() calls */
   s = ctime((time_t *)&temp);
#else
   s = (const char *) ctime_r((time_t *) &temp, str);
#endif
   if (s == nullptr) {
      return nullptr;
   }
   return sge_dstring_copy_string(buffer, s);
}

const char *sge_ctime64(u_long64 timestamp, dstring *dstr, bool is_xml, bool with_micro) {
   const char *ret;
   const std::chrono::microseconds us{timestamp};
   const std::chrono::seconds s = duration_cast<std::chrono::seconds>(us);
   time_t t = (time_t)s.count();
   struct tm tm{};

   if (localtime_r(&t, &tm) == nullptr) {
      ret = sge_strerror(errno, dstr);
   } else {
      // we could call the 32bit version of append_time here
      if (is_xml) {
         ret = sge_dstring_sprintf(dstr, "%04d-%02d-%02dT%02d:%02d:%02d",
                                   1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
                                   tm.tm_hour, tm.tm_min, tm.tm_sec);
      } else {
         ret = sge_dstring_sprintf(dstr, "%04d-%02d-%02d %02d:%02d:%02d",
                                   1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
                                   tm.tm_hour, tm.tm_min, tm.tm_sec);
      }
      if (with_micro) {
         long micro = us.count() % 1000000;
         ret = sge_dstring_sprintf_append(dstr, ".%06ld", micro);
      }
   }

   return ret;
}

const char *sge_ctime64(u_long64 timestamp, dstring *dstr) {
   return sge_ctime64(timestamp, dstr, false, true);
}

const char *sge_ctime64_short(u_long64 timestamp, dstring *dstr) {
   return sge_ctime64(timestamp, dstr, false, false);
}

const char *sge_ctime64_xml(u_long64 timestamp, dstring *dstr) {
   return sge_ctime64(timestamp, dstr, true, true);
}

/****** uti/time/sge_at_time() ************************************************
*  NAME
*     sge_at_time() -- ??? 
*
*  SYNOPSIS
*     const char* sge_at_time(time_t i, dstring *buffer) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     time_t i - 0 or time value 
*     dstring *buffer - buffer provided by caller
*
*  RESULT
*     const char* - time string (current time if 'i' was 0) 
*
*  NOTES
*     MT-NOTE: sge_at_time() is MT safe if localtime_r() can be used
*
*  SEE ALSO
*     uti/time/sge_ctime() 
******************************************************************************/
const char *sge_at_time(time_t i, dstring *buffer) {
#ifdef HAS_LOCALTIME_R
   struct tm tm_buffer{};
#endif
   struct tm *tm;

   if (!i)
      i = (time_t) sge_get_gmt();
#ifndef HAS_LOCALTIME_R
   tm = localtime(&i);
#else
   tm = (struct tm *) localtime_r(&i, &tm_buffer);
#endif
   return sge_dstring_sprintf(buffer, "%04d%02d%02d%02d%02d.%02d",
                              tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                              tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/****** uti/time/sge_stopwatch_log() ******************************************
*  NAME
*     sge_stopwatch_log() -- ??? 
*
*  SYNOPSIS
*     void sge_stopwatch_log(int i, const char *str) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     int i           - ??? 
*     const char *str - ??? 
*
*  NOTES
*     MT-NOTE: sge_stopwatch_log() is not MT safe due to access to global variables
*
*  SEE ALSO
*     uti/time/sge_stopwatch_start() 
******************************************************************************/
void sge_stopwatch_log(int i, const char *str) {
   if (i < 0 || i >= NESTLEVEL) {
      return;
   }
   if (time_log_interval[i] == -1) {
      return;
   }

   sge_stopwatch_stop(i);

   if ((wdiff[i] * 1000) / clock_tick >= time_log_interval[i]) {
      WARNING("%-30s: %d/%d/%d", str, (int) ((wtot[i] * 1000) / clock_tick), (int) ((end[i].tms_utime * 1000) / clock_tick), (int) ((end[i].tms_stime * 1000) / clock_tick));
   }
}

/****** uti/time/sge_stopwatch_start() ****************************************
*  NAME
*     sge_stopwatch_start() -- ??? 
*
*  SYNOPSIS
*     void sge_stopwatch_start(int i) 
*
*  FUNCTION
*     ??? 
*
*  INPUTS
*     int i - ??? 
*
*  NOTES
*     MT-NOTE: sge_stopwatch_start() is not MT safe due to access to global variables
*
*  SEE ALSO
*     uti/time/sge_stopwatch_log() 
******************************************************************************/
void sge_stopwatch_start(int i) {
   static int first = 1;
   int j;
   char *cp;

   if (first) {
      char buf[24];

      clock_tick = sysconf(_SC_CLK_TCK);
      for (j = 0; j < NESTLEVEL; j++) {
         wtot[j] = wbegin[j] = wprev[j] = wdiff[j] = 0;
         snprintf(buf, sizeof(buf), "SGE_TIMELOG%d", j);
         if ((cp = getenv(buf)) && (atoi(cp) >= 0)) {
            time_log_interval[j] = atoi(cp);
         } else {
            time_log_interval[j] = -1;
         }
      }
      first = 0;
   }

   if (i < 0 || i >= NESTLEVEL) {
      return;
   }
   if (time_log_interval[i] == -1) {
      return;
   }
   wbegin[i] = times(&begin[i]);

#ifdef TIMES_RETURNS_ZERO
   /* times() return 0 on these machines */
   wbegin[i] = (sge_get_gmt() - inittime) * clock_tick;
#endif

   wprev[i] = wbegin[i];
}

/****** uti/time/duration_add_offset() ****************************************
*  NAME
*     duration_add_offset() -- add function for time add
*
*  SYNOPSIS
*     u_long32 duration_add_offset(u_long32 duration, u_long32 offset) 
*
*  FUNCTION
*     add function to catch ulong overflow. Returns max ulong value if necessary
*
*  INPUTS
*     u_long32 duration - duration in seconds
*     u_long32 offset   - offset in seconds
*
*  RESULT
*     u_long32 - value < U_LONG32_MAX
*
*  NOTES
*     MT-NOTE: duration_add_offset() is not MT safe 
*******************************************************************************/
u_long64 duration_add_offset(u_long64 duration, u_long64 offset) {
   if (duration == U_LONG64_MAX || offset == U_LONG64_MAX) {
      return U_LONG64_MAX;
   }

   if ((U_LONG64_MAX - offset) < duration) {
      duration = U_LONG64_MAX;
   } else {
      duration += offset;
   }

   return duration;
}

/****** uti/time/sge_usleep() ****************************************
*  NAME
*     sge_usleep() -- Mimiks a non-iterruptable usleep() 
*
*  SYNOPSIS
*     void sge_usleep(int sleep_time) 
*
*  FUNCTION
*     Mimiks a non-iterruptable usleep() to the caller.
*
*  INPUTS
*     int sleep_time - requested sleep time
*
*  RESULT
*     n/a
*
*  NOTES
*     None.
*******************************************************************************/
void sge_usleep(int sleep_time) {
   struct timeval wake_tv{}, sleep_tv{}, snooze_tv{};
   int time_to_sleep = sleep_time;

   do {
      gettimeofday(&sleep_tv, nullptr);
      usleep(time_to_sleep);
      gettimeofday(&wake_tv, nullptr);
      if (wake_tv.tv_usec < sleep_tv.tv_usec) {
         wake_tv.tv_sec--;
         wake_tv.tv_usec = wake_tv.tv_usec + 1000000;
      }
      snooze_tv.tv_sec = wake_tv.tv_sec - sleep_tv.tv_sec;
      snooze_tv.tv_usec = wake_tv.tv_usec - sleep_tv.tv_usec;

      time_to_sleep = time_to_sleep - (snooze_tv.tv_sec * 1000000 + snooze_tv.tv_usec);
   } while (time_to_sleep > 0);

   return;
}
