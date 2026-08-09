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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Time formatting, parsing and monotonic time sources
 */

#include <ctime>
#include <chrono>
#include <sys/times.h>

#include <sys/time.h>

#include "uti/sge_dstring.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

/** @brief Current time as microseconds since the epoch
 * @return the current time in the 64 bit microsecond representation
 */
uint64_t sge_get_gmt64() {
   const auto now = std::chrono::system_clock::now();
   const auto epoch = now.time_since_epoch();
   const auto us = duration_cast<std::chrono::microseconds>(epoch);
   return us.count();
}

/** @brief Convert the 64 bit microsecond form to whole seconds
 * @param timestamp the instant in microseconds
 * @return seconds since the epoch, truncated
 */
uint32_t sge_gmt64_to_gmt32(uint64_t timestamp) {
   uint64_t ret = timestamp / 1000000;
   if (ret > std::numeric_limits<uint32_t>::max()) {
      ret = std::numeric_limits<uint32_t>::max();
   }
   return (uint32_t)ret;
}

/** @brief Convert the 64 bit microsecond form to a `time_t`
 * @param timestamp the instant in microseconds
 * @return the same instant as a `time_t`
 */
time_t sge_gmt64_to_time_t(uint64_t timestamp) {
   uint64_t ret = timestamp / 1000000;
   return (time_t)ret;
}

/** @brief Convert the 64 bit microsecond form to fractional seconds
 * @param timestamp the instant in microseconds
 * @return seconds, with the sub second part preserved
 */
double sge_gmt64_to_gmt32_double(uint64_t timestamp) {
   auto ret = (double)timestamp;
   return ret / 1000000.0;
}

/** @brief Convert a `time_t` to the 64 bit microsecond form
 * @param timestamp the instant as a `time_t`
 * @return the same instant in microseconds
 */
uint64_t sge_time_t_to_gmt64(time_t timestamp) {
   uint64_t ret = timestamp;
   return ret * 1000000;
}

/** @brief Convert the 64 bit microsecond form to a `struct timespec`
 * @param timestamp the instant in microseconds
 * @param[out] ts receives the same instant
 */
void sge_gmt64_to_timespec(uint64_t timestamp, struct timespec &ts) {
   ts.tv_sec = (time_t)(timestamp / 1000000);
   ts.tv_nsec = (long)(timestamp % 1000000) * 1000;
}

/** @brief Append a formatted timestamp to a buffer
 * @param timestamp the instant in microseconds
 * @param dstr buffer to append to
 * @param is_xml use the XML date layout
 * @return the formatted text held by the buffer
 */
const char *append_time(uint64_t timestamp, dstring *dstr, bool is_xml) {
   DSTRING_STATIC(local_dstr, 100);
   return sge_dstring_append(dstr, sge_ctime64(timestamp, &local_dstr, is_xml, true));
}

/**
 * @brief Convert time value into string
 *
 * Convert time value into string
 *
 * @param i time value
 * @param buffer dstring
 * @param is_xml write in XML dateTime format?
 *
 * @return time string (current time if 'i' was 0) dstring *buffer - buffer provided by caller
 *
 * @note MT-NOTE: append_time() is MT safe if localtime_r() can be used
 */
/** @overload */
const char *append_time(time_t i, dstring *buffer, bool is_xml) {
   const char *ret;
   struct tm tm_buffer{};
   auto *tm = (struct tm *) localtime_r(&i, &tm_buffer);

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

/** @brief Format a timestamp, choosing the layout explicitly
 * @param timestamp the instant to format
 * @param dstr buffer receiving the text
 * @param is_xml use the XML date layout instead of the human readable one
 * @param with_micro include the microsecond part
 * @return the formatted text held by the buffer
 */
const char *sge_ctime64(uint64_t timestamp, dstring *dstr, bool is_xml, bool with_micro) {
   const char *ret;

   if (timestamp == 0) {
      timestamp = sge_get_gmt64();
   }

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

/** @brief Format a timestamp in the default human readable layout
 * @param timestamp the instant to format
 * @param dstr buffer receiving the text
 * @return the formatted text held by the buffer
 */
const char *sge_ctime64(uint64_t timestamp, dstring *dstr) {
   return sge_ctime64(timestamp, dstr, false, true);
}

/** @brief Format a timestamp in the short layout
 * @param timestamp the instant to format
 * @param dstr buffer receiving the text
 * @return the formatted text held by the buffer
 */
const char *sge_ctime64_short(uint64_t timestamp, dstring *dstr) {
   return sge_ctime64(timestamp, dstr, false, false);
}

/** @brief Format a timestamp in the XML layout
 * @param timestamp the instant to format
 * @param dstr buffer receiving the text
 * @return the formatted text held by the buffer
 */
const char *sge_ctime64_xml(uint64_t timestamp, dstring *dstr) {
   return sge_ctime64(timestamp, dstr, true, true);
}

/** @brief Format a timestamp as date and time without the sub second part
 * @param timestamp the instant to format
 * @param dstr buffer receiving the text
 * @return the formatted text held by the buffer
 */
const char *sge_ctime64_date_time(uint64_t timestamp, dstring *dstr) {
   const char *ret;

   if (timestamp == 0) {
      timestamp = sge_get_gmt64();
   }

   const std::chrono::microseconds us{timestamp};
   const std::chrono::seconds s = duration_cast<std::chrono::seconds>(us);
   time_t t = (time_t)s.count();
   struct tm tm{};

   if (localtime_r(&t, &tm) == nullptr) {
      ret = sge_strerror(errno, dstr);
   } else {
      ret = sge_dstring_sprintf(dstr, "%04d%02d%02d%02d%02d.%02d",
                                1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
                                tm.tm_hour, tm.tm_min, tm.tm_sec);
   }

   return ret;
}

/** @brief Wait until an absolute point in time
 *
 * Sleeps until @p then, returning early only if interrupted.
 *
 * @param i the instant to wait for, as a `time_t`
 * @param buffer buffer receiving the formatted time
 * @return the formatted time held by @p buffer
 */
const char *sge_at_time(time_t i, dstring *buffer) {
   struct tm tm_buffer{};

   if (!i)
      i = time(nullptr);
   auto *tm = (struct tm *) localtime_r(&i, &tm_buffer);
   return sge_dstring_sprintf(buffer, "%04d%02d%02d%02d%02d.%02d",
                              tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                              tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/**
 * @brief Add function for time add
 *
 * add function to catch ulong overflow. Returns max ulong value if necessary
 *
 * @param duration duration in seconds
 * @param offset offset in seconds
 *
 * @return value < std::numeric_limits<uint32_t>::max()
 *
 * @note MT-NOTE: duration_add_offset() is not MT safe
 */
uint64_t duration_add_offset(uint64_t duration, uint64_t offset) {
   if (duration == std::numeric_limits<uint64_t>::max() || offset == std::numeric_limits<uint64_t>::max()) {
      return std::numeric_limits<uint64_t>::max();
   }

   if ((std::numeric_limits<uint64_t>::max() - offset) < duration) {
      duration = std::numeric_limits<uint64_t>::max();
   } else {
      duration += offset;
   }

   return duration;
}

/**
 * @brief Mimiks a non-iterruptable usleep()
 *
 * Mimiks a non-iterruptable usleep() to the caller.
 *
 * @param sleep_time requested sleep time
 *
 * @note None.
 */
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
