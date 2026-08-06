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
 * @brief Implementation of runtime profiling, see @ref uti_profiling
 */

#include <iostream>
#include <sys/times.h>
#include <cstring>
#include <unistd.h>
#include <pthread.h>

#include "uti/msg_utilib.h"
#include "uti/sge_dstring.h"
#include "uti/sge_profiling.h"
#include "uti/sge_log.h"
#include "uti/sge_lock_fifo.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_stdlib.h"

/** @brief Maps a thread id to its name, and remembers whether it profiles
 *
 * One entry per thread in the process wide profiling array.
 */
typedef struct {
   const char *thrd_name;   ///< name of the thread
   pthread_t thrd_id;       ///< id of the thread
   bool prof_is_active;     ///< true while this thread collects profiling data
   int is_terminated;       ///< non-zero once the thread has ended, so the slot can be reused
} sge_thread_info_t;

static int sge_prof_array_initialized = 0;

static void prof_info_init(prof_level level, pthread_t thread_id);

static void prof_info_level_init(prof_level i, int thread_num);

static void prof_reset_thread(int thread_num, prof_level level);

static void init_array(pthread_t num);

static void init_array_first();

static void init_thread_info();

static int get_prof_info_thread_id(pthread_t thread_num);

static void sge_prof_cleanup();

static sge_prof_info_t **theInfo = nullptr;
static sge_thread_info_t *thrdInfo = nullptr;

static pthread_mutex_t thrdInfo_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t thread_id_key;

static bool profiling_enabled = true;

static pthread_once_t prof_once = PTHREAD_ONCE_INIT;

/// size of the profiling array, i.e. the largest number of threads that can profile
constexpr int MAX_THREAD_NUM = MAX_TOTAL_COMPONENT_THREADS;

/**
 * @brief Enables/disables profiling
 *
 * Enables/disables profiling completely.  Profiling is enabled by
 * default.  This method is the fix to Issue 1471.
 *
 * @param enabled true to enable profiling, false to disable it
 *
 * @note MT-NOTE: sge_prof_set_enabled() is MT safe only if called before any other
 *       profiling calls
 */
void sge_prof_set_enabled(bool enabled) {
   profiling_enabled = enabled;
}

/**
 * @brief Inititalizes the profiling array
 *
 * Initializes the profiling array.
 *
 * @note MT-NOTE: prof_thread_local_once_init() is MT safe only if called before any other
 *       profiling calls other than sge_prof_set_enabled()
 */
static void prof_thread_local_once_init() {
   if (!profiling_enabled) {
      return;
   }

   init_thread_info();
   init_array_first();
   init_array(pthread_self());
}

static void
prof_mt_init() {
   pthread_once(&prof_once, prof_thread_local_once_init);
}

/** @brief Runs `prof_mt_init()` once per thread, from its constructor
 *
 * A thread local instance makes the per thread profiling setup happen on first
 * use, without every thread having to remember to call it.
 */
class ProfThreadInit {
public:
   ProfThreadInit() {
      prof_mt_init();
   }
   ~ProfThreadInit() {
      sge_prof_cleanup();
   }
};

// although not used the constructor call has the side effect to initialize the pthread_key => do not delete
static ProfThreadInit prof_obj{};


/**
 * @brief Set name of a custom level
 *
 * Set the name of a custom profiling level.
 *
 * @param level level to edit
 * @param name new name for level
 * @param error if != nullptr, error messages will be put here
 *
 * @return true on success, else false is returned and an error message is returned in parameter error, if error != nullptr
 *
 * @note MT-NOTE: prof_set_level_name() is MT safe
 */
bool prof_set_level_name(prof_level level, const char *name, dstring *error) {
   pthread_t thread_id;
   int thread_num;
   bool ret = true;

   if (level >= SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_set_level_name", level);
      ret = false;
   } else if (name == nullptr) {
      sge_dstring_sprintf_append(error, MSG_PROF_NULLLEVELNAME_S, "prof_set_level_name");
      ret = false;
   } else {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         ret = false;
      } else {
         theInfo[thread_num][level].name = name;
      }
   }

   return ret;
}

/**
 * @brief Is profiling active?
 *
 * Returns true, if profiling is active, else false.
 *
 * @param level the level to query, see @ref uti_profiling_levels
 *
 * @return true when profiling is active for @p level
 *
 * @note MT-NOTE: prof_is_active() is MT safe
 *
 * @see #prof_stop
 */
bool prof_is_active(prof_level level) {
   int thread_num;
   pthread_t thread_id;
   bool ret = false;

   if (profiling_enabled && (level <= SGE_PROF_ALL)) {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);
      if ((thread_num >= 0) && (thread_num < MAX_THREAD_NUM)) {
         ret = theInfo[thread_num][level].prof_is_started;
      }
   }

   return ret;
}

/**
 * @brief Start profiling
 *
 * Enables profiling. All internal variables are reset to 0.
 * Performance measurement has to be started and stopped by
 * calling profiling_start_measurement or profiling_stop_measurement.
 *
 * @param level the level to operate on, see @ref uti_profiling_levels
 * @param error if != nullptr, error messages will be put here
 *
 * @return true on success, else false is returned and an error message is returned in parameter error, if error != nullptr
 *
 * @note MT-NOTE: prof_start() is MT safe
 *
 * @see #prof_stop, #prof_start_measurement, #prof_stop_measurement
 */
bool prof_start(prof_level level, dstring *error) {
   pthread_t thread_id;
   int thread_num;
   bool ret = true;

   if (level > SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_start", level);
      ret = false;
   } else if (profiling_enabled) {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         sge_dstring_sprintf_append(error, MSG_PROF_MAXTHREADSEXCEEDED_S, "prof_start");
         ret = false;
      } else if (theInfo[thread_num][level].prof_is_started) {
         sge_dstring_sprintf_append(error, MSG_PROF_ALREADYACTIVE_S, "prof_start");
         ret = false;
      } else {
         struct tms tms_buffer{};
         int i;
         clock_t start_time = times(&tms_buffer);

         if (level == SGE_PROF_ALL) {
            for (i = SGE_PROF_OTHER; i <= SGE_PROF_ALL; i++) {
               /* get start time */
               theInfo[thread_num][i].start_clock = start_time;

               /* initialize names and reset all data */
               ret = prof_reset((prof_level)i, error);

               theInfo[thread_num][i].prof_is_started = true;

               theInfo[thread_num][i].ever_started = true;
            }
         } else {
            /* get start time */
            theInfo[thread_num][level].start_clock = start_time;

            /* initialize names and reset all data */
            ret = prof_reset(level, error);

            theInfo[thread_num][level].prof_is_started = true;
            /* TODO
             * Is there a reason we do this?  This flag in never cleared. */
            theInfo[thread_num][SGE_PROF_ALL].prof_is_started = true;

            theInfo[thread_num][level].ever_started = true;
         }

         /* we have no actual profiling level */
         theInfo[thread_num][SGE_PROF_ALL].akt_level = SGE_PROF_NONE;

         /* implicitly start the OTHER level */
         prof_start_measurement(SGE_PROF_OTHER, error);
      }
   }

   return ret;
}

/**
 * @brief Stop profiling
 *
 * Profiling is disabled.
 *
 * Subsequent calls to profiling_start_measurement or
 * profiling_stop_measurement will have no effect.
 *
 * Profiling can be re-enabled by calling profiling_start.
 *
 * @param level the level to operate on, see @ref uti_profiling_levels
 * @param error if != nullptr, error messages will be put here
 *
 * @return true on success, else false is returned and an error message is returned in parameter error, if error != nullptr
 *
 * @note MT-NOTE: prof_start() is MT safe
 *
 * @see #prof_start, #prof_start_measurement, #prof_stop_measurement
 */
bool prof_stop(prof_level level, dstring *error) {
   pthread_t thread_id;
   int thread_num;
   bool ret = true;
   int i;

   if (level > SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_stop", level);
      ret = false;
   } else if (profiling_enabled) {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         sge_dstring_sprintf_append(error, MSG_PROF_MAXTHREADSEXCEEDED_S, "prof_stop");
         ret = false;
      } else if (!theInfo[thread_num][level].prof_is_started) {
         sge_dstring_sprintf_append(error, MSG_PROF_NOTACTIVE_S, "prof_stop");
         ret = false;
      } else {
         prof_stop_measurement(SGE_PROF_OTHER, error);

         if (level == SGE_PROF_ALL) {
            for (i = SGE_PROF_OTHER; i <= SGE_PROF_ALL; i++) {
               theInfo[thread_num][i].prof_is_started = false;
            }
         } else {
            theInfo[thread_num][level].prof_is_started = false;
         }
      }
   }

   return ret;
}

/** @brief Shared implementation of #prof_start and #prof_stop
 *
 * @param level the level to start or stop, see @ref uti_profiling_levels
 * @param error if not nullptr, an error message is appended here
 * @param do_start true to start profiling, false to stop it
 */
void
prof_start_stop(prof_level level, dstring *error, bool do_start) {
   if (do_start) {
      prof_start(level, error);
   } else {
      prof_stop(level, error);
   }
}

/**
 * @brief Start measurement
 *
 * Starts measurement of performance data.
 * Retrieves and stores current time and usage information.
 *
 * @param level level to process
 * @param error if != nullptr, error messages will be put here
 *
 * @return true on success, else false is returned and an error message is returned in parameter error, if error != nullptr
 *
 * @note MT-NOTE: prof_start_measurement() is MT safe
 *
 * @see #prof_stop_measurement
 */
bool prof_start_measurement(prof_level level, dstring *error) {
   pthread_t thread_id;
   int thread_num;
   bool ret = true;

   if (level >= SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_start_measurement", level);
      ret = false;
   } else if (profiling_enabled) {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         sge_dstring_sprintf_append(error, MSG_PROF_MAXTHREADSEXCEEDED_S, "prof_start_measurement");
         ret = false;
      } else if (!theInfo[thread_num][level].prof_is_started) {
         sge_dstring_sprintf_append(error, MSG_PROF_NOTACTIVE_S, "prof_start_measurement");
         ret = false;
      } else if (theInfo[thread_num][SGE_PROF_ALL].akt_level == level) {
         /* multiple start_measurement calls within one level are allowed */
         theInfo[thread_num][level].nested_calls++;
      } else if (theInfo[thread_num][level].pre != SGE_PROF_NONE) {
         /* we cannot yet handle cyclic measurements between multiple levels
          * produce an error and stop profiling
          */
         sge_dstring_sprintf_append(error, MSG_PROF_CYCLICNOTALLOWED_SD, "prof_start_measurement", level);
         prof_stop(level, error);
         ret = false;
      } else {
         theInfo[thread_num][level].pre = theInfo[thread_num][SGE_PROF_ALL].akt_level;
         theInfo[thread_num][SGE_PROF_ALL].akt_level = level;

         theInfo[thread_num][level].start = times(&(theInfo[thread_num][level].tms_start));
         /* when we start a level, we have no sub usage */
         theInfo[thread_num][level].sub = 0;
         theInfo[thread_num][level].sub_utime = 0;
         theInfo[thread_num][level].sub_utime = 0;
      }
   }

   return ret;
}

/**
 * @brief Stop measurement
 *
 * Stops measurement for a certain code block.
 * Retrieves and stores current time and usage information.
 * Sums up global usage information.
 *
 * @param level level to process
 * @param error if != nullptr, error messages will be put here
 *
 * @return true on success, else false is returned and an error message is returned in parameter error, if error != nullptr
 *
 * @note MT-NOTE: prof_stop_measurement() is MT safe
 *
 * @see #prof_start_measurement
 */
bool prof_stop_measurement(prof_level level, dstring *error) {
   pthread_t thread_id;
   int thread_num;
   bool ret = true;

   if (level >= SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_stop_measurement", level);
      ret = false;
   } else if (profiling_enabled) {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         sge_dstring_sprintf_append(error, MSG_PROF_MAXTHREADSEXCEEDED_S, "prof_stop_measurement");
         ret = false;
      } else if (!theInfo[thread_num][level].prof_is_started) {
         sge_dstring_sprintf_append(error, MSG_PROF_NOTACTIVE_S, "prof_stop_measurement");
         ret = false;
      } else {
         clock_t time, utime, stime;

         if (theInfo[thread_num][level].nested_calls > 0) {
            theInfo[thread_num][level].nested_calls--;
         } else {
            theInfo[thread_num][level].end = times(&(theInfo[thread_num][level].tms_end));
            time = theInfo[thread_num][level].end - theInfo[thread_num][level].start;
            utime = theInfo[thread_num][level].tms_end.tms_utime - theInfo[thread_num][level].tms_start.tms_utime;
            stime = theInfo[thread_num][level].tms_end.tms_stime - theInfo[thread_num][level].tms_start.tms_stime;

#if 0
            if (time < (utime + stime)) {
               DPRINTF("---> utime + stime > time, difference is %d clock ticks\n", (utime + stime) - time);
            }
#endif
            theInfo[thread_num][level].total += time;
            theInfo[thread_num][level].total_utime += utime;
            theInfo[thread_num][level].total_stime += stime;


            if (theInfo[thread_num][level].pre != SGE_PROF_NONE) {
               prof_level pre = theInfo[thread_num][level].pre;

               theInfo[thread_num][pre].sub += time;
               theInfo[thread_num][pre].sub_utime += utime;
               theInfo[thread_num][pre].sub_stime += stime;

               theInfo[thread_num][pre].sub_total += time;
               theInfo[thread_num][pre].sub_total_utime += utime;
               theInfo[thread_num][pre].sub_total_stime += stime;

               theInfo[thread_num][SGE_PROF_ALL].akt_level = theInfo[thread_num][level].pre;
               theInfo[thread_num][level].pre = SGE_PROF_NONE;
            } else {
               theInfo[thread_num][SGE_PROF_ALL].akt_level = SGE_PROF_NONE;
            }
         }
      }
   }

   return ret;
}

/**
 * @brief Reset usage information
 *
 * Reset usage and timing information to 0.
 *
 * @param level the level to operate on, see @ref uti_profiling_levels
 * @param error if != nullptr, error messages will be put here
 *
 * @return true on success, else false is returned and an error message is returned in parameter error, if error != nullptr
 *
 * @note MT-NOTE: prof_reset() is MT safe
 */
bool prof_reset(prof_level level, dstring *error) {
   pthread_t thread_id;
   int thread_num;
   bool ret = true;

   if (level > SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_reset", level);
      ret = false;
   } else if (profiling_enabled) {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         sge_dstring_sprintf_append(error, MSG_PROF_MAXTHREADSEXCEEDED_S, "prof_reset");
         ret = false;
      } else if (theInfo[thread_num][SGE_PROF_ALL].akt_level > SGE_PROF_OTHER) {
         sge_dstring_sprintf_append(error, MSG_PROF_RESETWHILEMEASUREMENT_S, "prof_reset");
         ret = false;
      } else {
         if (theInfo[thread_num][level].prof_is_started) {
            ret = prof_stop_measurement(SGE_PROF_OTHER, error);
         }
         if (level == SGE_PROF_ALL) {
            int c;
            for (c = SGE_PROF_OTHER; c <= SGE_PROF_ALL; c++) {
               prof_reset_thread(thread_num, (prof_level)c);
            }
         } else {
            prof_reset_thread(thread_num, level);
         }

         if (theInfo[thread_num][level].prof_is_started) {
            ret = prof_start_measurement(SGE_PROF_OTHER, error);
         }
      }
   }

   return ret;
}

/**
 * @brief Reset usage information
 *
 * Reset usage and timing information for a single thread and level to 0.
 *
 * @note MT-NOTE: prof_reset() is MT safe
 */
static void prof_reset_thread(int thread_num, prof_level level) {
   struct tms tms_buffer{};

   theInfo[thread_num][level].start = 0;
   theInfo[thread_num][level].end = 0;
   theInfo[thread_num][level].tms_start.tms_utime = 0;
   theInfo[thread_num][level].tms_start.tms_stime = 0;
   theInfo[thread_num][level].tms_start.tms_cutime = 0;
   theInfo[thread_num][level].tms_start.tms_cstime = 0;
   theInfo[thread_num][level].tms_end.tms_utime = 0;
   theInfo[thread_num][level].tms_end.tms_stime = 0;
   theInfo[thread_num][level].tms_end.tms_cutime = 0;
   theInfo[thread_num][level].tms_end.tms_cstime = 0;
   theInfo[thread_num][level].total = 0;
   theInfo[thread_num][level].total_utime = 0;
   theInfo[thread_num][level].total_stime = 0;

   theInfo[thread_num][level].pre = SGE_PROF_NONE;
   theInfo[thread_num][level].sub = 0;
   theInfo[thread_num][level].sub_utime = 0;
   theInfo[thread_num][level].sub_stime = 0;
   theInfo[thread_num][level].sub_total = 0;
   theInfo[thread_num][level].sub_total_utime = 0;
   theInfo[thread_num][level].sub_total_stime = 0;

   theInfo[thread_num][level].start_clock = times(&tms_buffer);
}

/**
 * @brief Return wallclock of a measurement
 *
 * Returns the wallclock of the last measurement in seconds.
 * Resolution is clock ticks (_SC_CLK_TCK).
 *
 * @param level level to process
 * @param with_sub include usage of subordinated measurements?
 * @param error if != nullptr, error messages will be put here
 *
 * @return the wallclock time of the last measurement on error, 0 is returned and an error message is written to the buffer given in parameter error, if error != nullptr double - the wallclock time
 *
 * @note MT-NOTE: prof_get_measurement_wallclock() is MT safe
 */
double prof_get_measurement_wallclock(prof_level level, bool with_sub, dstring *error) {
   pthread_t thread_id;
   int thread_num;
   clock_t clock = 0;
   double ret = 0.0;

   if (level >= SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_get_measurement_wallclock", level);
   } else if (profiling_enabled) {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         sge_dstring_sprintf_append(error, MSG_PROF_MAXTHREADSEXCEEDED_S, "prof_get_measurement_wallclock");
      } else {
         clock = theInfo[thread_num][level].end - theInfo[thread_num][level].start;

         if (!with_sub) {
            clock -= theInfo[thread_num][level].sub;
         }
      }

      ret = clock * 1.0 / sysconf(_SC_CLK_TCK);
   }

   return ret;
}

/**
 * @brief Return user cpu time of measurement
 *
 * Returns the user cpu time of the last measurement in seconds.
 * Resolution is clock ticks (_SC_CLK_TCK).
 *
 * @param level level to process
 * @param with_sub include usage of subordinated measurements?
 * @param error if != nullptr, error messages will be put here
 *
 * @return the user cpu time of the last measurement on error, 0 is returned and an error message is written to the buffer given in parameter error, if error != nullptr
 *
 * @note MT-NOTE: prof_get_measurement_utime() is not MT safe
 */
double prof_get_measurement_utime(prof_level level, bool with_sub, dstring *error) {
   pthread_t thread_id;
   int thread_num;
   clock_t clock = 0;
   double ret = 0.0;

   if (level >= SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_get_measurement_utime", level);
   } else if (profiling_enabled) {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         sge_dstring_sprintf_append(error, MSG_PROF_MAXTHREADSEXCEEDED_S, "prof_get_measurement_utime");
      } else {
         clock = (theInfo[thread_num][level].tms_end.tms_utime - theInfo[thread_num][level].tms_start.tms_utime);

         if (!with_sub) {
            clock -= theInfo[thread_num][level].sub_utime;
         }
      }

      ret = clock * 1.0 / sysconf(_SC_CLK_TCK);
   }

   return ret;
}

/**
 * @brief Return system cpu time of measurement
 *
 * Returns the system cpu time of the last measurement in seconds.
 * Resolution is clock ticks (_SC_CLK_TCK).
 *
 * @param level level to process
 * @param with_sub include usage of subordinated measurements?
 * @param error if != nullptr, error messages will be put here
 *
 * @return the system cpu time of the last measurement on error, 0 is returned and an error message is written to the buffer given in parameter error, if error != nullptr
 *
 * @note MT-NOTE: prof_get_measurement_stime() is MT safe
 */
double prof_get_measurement_stime(prof_level level, bool with_sub, dstring *error) {
   pthread_t thread_id;
   int thread_num;
   clock_t clock = 0;
   double ret = 0.0;

   if (level >= SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_get_measurement_stime", level);
   } else if (profiling_enabled) {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         sge_dstring_sprintf_append(error, MSG_PROF_MAXTHREADSEXCEEDED_S, "prof_get_measurement_stime");
      } else {
         clock = (theInfo[thread_num][level].tms_end.tms_stime - theInfo[thread_num][level].tms_start.tms_stime);

         if (!with_sub) {
            clock -= theInfo[thread_num][level].sub_stime;
         }
      }

      ret = clock * 1.0 / sysconf(_SC_CLK_TCK);
   }

   return ret;
}

/**
 * @brief Get total wallclock time
 *
 * Returns the wallclock time since profiling was enabled in seconds.
 * Resolution is clock ticks (_SC_CLK_TCK).
 *
 * @param level the level to operate on, see @ref uti_profiling_levels
 * @param error if != nullptr, error messages will be put here
 *
 * @return the total wallclock time of the profiling run on error, 0 is returned and an error message is written to the buffer given in parameter error, if error != nullptr
 *
 * @note MT-NOTE: prof_get_total_wallclock() is MT safe
 */

double prof_get_total_wallclock(prof_level level, dstring *error) {
   pthread_t thread_id;
   int thread_num;
   double ret = 0.0;

   if (level >= SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_get_total_wallclock", level);
   } else if (profiling_enabled) {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         sge_dstring_sprintf_append(error, MSG_PROF_MAXTHREADSEXCEEDED_S, "prof_get_total_wallclock");
      } else if (!theInfo[thread_num][level].prof_is_started) {
         sge_dstring_sprintf_append(error, MSG_PROF_NOTACTIVE_S, "prof_get_total_wallclock");
      } else {
         struct tms tms_buffer{};
         clock_t now;

         now = times(&tms_buffer);

         ret = (now - theInfo[thread_num][level].start_clock) * 1.0 / sysconf(_SC_CLK_TCK);
      }
   }

   return ret;
}

/**
 * @brief Return total busy time
 *
 * Returns the total busy time since profiling was enabled in seconds.
 * Busy time is the time between starting and stopping a measurement.
 * Resolution is clock ticks (_SC_CLK_TCK).
 *
 * @param level level to process
 * @param with_sub include usage of subordinated measurements?
 * @param error if != nullptr, error messages will be put here
 *
 * @return the total busy time of the profiling run on error, 0 is returned and an error message is written to the buffer given in parameter error, if error != nullptr
 *
 * @note MT-NOTE: prof_get_total_busy() is MT safe
 */
static double _prof_get_total_busy(prof_level level, bool with_sub, dstring *error) {
   pthread_t thread_id = pthread_self();
   int thread_num;
   clock_t clock = 0;

   thread_num = get_prof_info_thread_id(thread_id);

   clock = theInfo[thread_num][level].total;

   if (!with_sub) {
      clock -= theInfo[thread_num][level].sub_total;
   }

   return clock * 1.0 / sysconf(_SC_CLK_TCK);
}

/** @brief Total busy time of a level
 *
 * Wallclock time during which a measurement of @p level was running, summed
 * over every measurement since profiling started.
 *
 * @param level the level to query, see @ref uti_profiling_levels
 * @param with_sub include the time spent in nested measurements
 * @param error if not nullptr, an error message is appended here
 * @return the time in seconds, or 0 on error
 */
double prof_get_total_busy(prof_level level, bool with_sub, dstring *error) {
   double ret = 0.0;
   pthread_t thread_id;
   int thread_num;

   if (level > SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_get_total_busy", level);
   } else if (profiling_enabled) {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         sge_dstring_sprintf_append(error, MSG_PROF_MAXTHREADSEXCEEDED_S, "prof_get_total_busy");
      } else if (level == SGE_PROF_ALL) {
         int i;

         for (i = SGE_PROF_OTHER; i < SGE_PROF_ALL; i++) {
            ret += _prof_get_total_busy((prof_level)i, with_sub, error);
         }
      } else {
         ret = _prof_get_total_busy(level, with_sub, error);
      }
   }

   return ret;
}

/**
 * @brief Get total user cpu time
 *
 * Returns the user cpu time since profiling was enabled in seconds.
 * Resolution is clock ticks (_SC_CLK_TCK).
 *
 * @param level level to process
 * @param with_sub include usage of subordinated measurements?
 * @param error if != nullptr, error messages will be put here
 *
 * @return the total user cpu time of the profiling run on error, 0 is returned and an error message is written to the buffer given in parameter error, if error != nullptr
 *
 * @note MT-NOTE: prof_get_total_utime() is MT safe
 */
static double _prof_get_total_utime(prof_level level, bool with_sub, dstring *error) {
   pthread_t thread_id = pthread_self();
   int thread_num;
   clock_t clock = 0;

   thread_num = get_prof_info_thread_id(thread_id);

   clock = theInfo[thread_num][level].total_utime;

   if (!with_sub) {
      clock -= theInfo[thread_num][level].sub_total_utime;
   }

   return clock * 1.0 / sysconf(_SC_CLK_TCK);
}

/** @brief Total user CPU time of a level
 *
 * @param level the level to query, see @ref uti_profiling_levels
 * @param with_sub include the time spent in nested measurements
 * @param error if not nullptr, an error message is appended here
 * @return the time in seconds, or 0 on error
 */
double prof_get_total_utime(prof_level level, bool with_sub, dstring *error) {
   double ret = 0.0;
   pthread_t thread_id;
   int thread_num;

   if (level > SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_get_total_utime", level);
   } else if (profiling_enabled) {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         sge_dstring_sprintf_append(error, MSG_PROF_MAXTHREADSEXCEEDED_S, "prof_get_total_utime");
      } else if (level == SGE_PROF_ALL) {
         int i;

         for (i = SGE_PROF_OTHER; i < SGE_PROF_ALL; i++) {
            ret += _prof_get_total_utime((prof_level)i, with_sub, error);
         }
      } else {
         ret = _prof_get_total_utime(level, with_sub, error);
      }
   }

   return ret;
}

/**
 * @brief Get total system cpu time
 *
 * Returns the total system cpu time since profiling was enabled in seconds.
 * Resolution is clock ticks (_SC_CLK_TCK).
 *
 * @param level level to process
 * @param with_sub include usage of subordinated measurements?
 * @param error if != nullptr, error messages will be put here
 *
 * @return the total system cpu time of the profiling run on error, 0 is returned and an error message is written to the buffer given in parameter error, if error != nullptr
 *
 * @note MT-NOTE: prof_get_total_stime() is MT safe
 */
static double _prof_get_total_stime(prof_level level, bool with_sub, dstring *error) {
   pthread_t thread_id = pthread_self();
   int thread_num;
   clock_t clock = 0;

   thread_num = get_prof_info_thread_id(thread_id);

   clock = theInfo[thread_num][level].total_stime;

   if (!with_sub) {
      clock -= theInfo[thread_num][level].sub_total_stime;
   }

   return clock * 1.0 / sysconf(_SC_CLK_TCK);
}

/** @brief Total system CPU time of a level
 *
 * @param level the level to query, see @ref uti_profiling_levels
 * @param with_sub include the time spent in nested measurements
 * @param error if not nullptr, an error message is appended here
 * @return the time in seconds, or 0 on error
 */
double prof_get_total_stime(prof_level level, bool with_sub, dstring *error) {
   double ret = 0.0;
   pthread_t thread_id;
   int thread_num;

   if (level > SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_get_total_stime", level);
   } else if (profiling_enabled) {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         sge_dstring_sprintf_append(error, MSG_PROF_MAXTHREADSEXCEEDED_S, "prof_get_total_stime");
      } else if (level == SGE_PROF_ALL) {
         int i;

         for (i = SGE_PROF_OTHER; i < SGE_PROF_ALL; i++) {
            ret += _prof_get_total_stime((prof_level)i, with_sub, error);
         }
      } else {
         ret = _prof_get_total_stime(level, with_sub, error);
      }
   }

   return ret;
}

/**
 * @brief Get informational message
 *
 * Returns a string containing the most interesting data, both for the
 * last measurement and for the total runtime:
 *    - wallclock of measurement
 *    - user cpu time of measurement
 *    - system cpu time of measurement
 *    - total wallclock time (runtime)
 *    - total busys time
 *    - utilization (busy time / wallclock time) * 100 %
 *
 * @code
 * The result can look like the following:
 * "wc = 0.190s, utime = 0.120s, stime = 0.000s, runtime 9515s, busy 105s,
 * utilization 1%"
 * @endcode
 *
 * @param level level to process
 * @param with_sub include usage of subordinated measurements?
 * @param error if != nullptr, error messages will be put here
 *
 * @return pointer to result string. It is valid until the next call of prof_get_info_string() on error, 0 is returned and an error message is written to the buffer given in parameter error, if error != nullptr
 *
 * @note MT-NOTE: prof_get_info_string() is MT safe
 */

#define PROF_GET_INFO_FORMAT "%-15.15s: wc = %10.3fs, utime = %10.3fs, stime = %10.3fs, utilization = %3.0f%%\n"

static const char *
_prof_get_info_string(prof_level level, dstring *info_string, bool with_sub, dstring *error) {
   pthread_t thread_id = pthread_self();
   int thread_num;
   dstring level_string = DSTRING_INIT;
   double busy, utime, stime, utilization;

   thread_num = get_prof_info_thread_id(thread_id);

   busy = prof_get_total_busy(level, with_sub, error);
   utime = prof_get_total_utime(level, with_sub, error);
   stime = prof_get_total_stime(level, with_sub, error);
   utilization = busy > 0 ? (utime + stime) / busy * 100 : 0;

   sge_dstring_sprintf(&level_string, PROF_GET_INFO_FORMAT,
                       theInfo[thread_num][level].name, busy, utime, stime, utilization);

   const char *ret = sge_dstring_append_dstring(info_string, &level_string);
   sge_dstring_free(&level_string);

   return ret;
}


const char *
/** @brief Formatted summary of a level, ready to print
 *
 * @param level the level to report, see @ref uti_profiling_levels
 * @param with_sub include the time spent in nested measurements
 * @param error if not nullptr, an error message is appended here
 * @return the summary string, valid until the next call for the same thread,
 *         or nullptr on error
 */
prof_get_info_string(prof_level level, bool with_sub, dstring *error) {
   pthread_t thread_id;
   int thread_num;
   const char *ret = nullptr;

   if (level > SGE_PROF_ALL) {
      sge_dstring_sprintf_append(error, MSG_PROF_INVALIDLEVEL_SD, "prof_get_info_string", level);
      ret = sge_dstring_get_string(error);
   } else if (!profiling_enabled) {
      ret = "Profiling disabled";
   } else {
      thread_id = pthread_self();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num < 0) || (thread_num >= MAX_THREAD_NUM)) {
         sge_dstring_sprintf_append(error, MSG_PROF_MAXTHREADSEXCEEDED_S, "prof_get_info_string");
         /* total summary: one line for each level, one line for summary */
      } else if (level == SGE_PROF_ALL) {
         double busy, utime, stime, utilization;
         int i;
         dstring total_string = DSTRING_INIT;

         for (i = SGE_PROF_OTHER; i <= SGE_PROF_ALL; i++) {
            /* clear previous contents */
            sge_dstring_clear(&(theInfo[thread_num][i].info_string));
         }

         prof_stop_measurement(SGE_PROF_OTHER, error);

         busy = prof_get_total_busy(SGE_PROF_ALL, with_sub, error);
         utime = prof_get_total_utime(SGE_PROF_ALL, with_sub, error);
         stime = prof_get_total_stime(SGE_PROF_ALL, with_sub, error);
         utilization = busy > 0 ? (utime + stime) / busy * 100 : 0;

         for (i = SGE_PROF_OTHER; i < SGE_PROF_ALL; i++) {
            if (theInfo[thread_num][i].name != nullptr && theInfo[thread_num][i].ever_started) {
               _prof_get_info_string((prof_level)i, &theInfo[thread_num][SGE_PROF_ALL].info_string, with_sub, error);
            }
         }

         prof_start_measurement(SGE_PROF_OTHER, error);

         sge_dstring_sprintf(&total_string, PROF_GET_INFO_FORMAT,
                             "total", busy, utime, stime, utilization, level);

         ret = sge_dstring_append_dstring(&theInfo[thread_num][SGE_PROF_ALL].info_string, &total_string);

         sge_dstring_free(&total_string);
      } else {

         /* clear previous contents */
         sge_dstring_clear(&(theInfo[thread_num][level].info_string));

         if (theInfo[thread_num][level].name != nullptr) {
            ret = _prof_get_info_string(level, &theInfo[thread_num][level].info_string, with_sub, error);
         }
      }
   }

   return ret;
}


/** @brief Write the profiling summary of a level to the message file
 *
 * @param level the level to report, see @ref uti_profiling_levels
 * @param with_sub include the time spent in nested measurements
 * @param info text put in front of the figures, to say what was measured
 * @return true when something was written, false when profiling is not active
 */
bool prof_output_info(prof_level level, bool with_sub, const char *info) {
   bool ret = false;

   DENTER(TOP_LAYER);

   if (profiling_enabled && (level <= SGE_PROF_ALL)) {
      int thread_num;
      pthread_t thread_id;

      thread_id = pthread_self();
      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num >= 0) && (thread_num < MAX_THREAD_NUM) && prof_is_active(level)) {
         struct saved_vars_s *context = nullptr;

         const char *info_message = prof_get_info_string(level, with_sub, nullptr);
         PROFILING("PROF(%d): %s%s", (int) thread_num, info, "");
         for (const char *message = sge_strtok_r(info_message, "\n", &context); message != nullptr;
              message = sge_strtok_r(nullptr, "\n", &context)) {
            PROFILING("PROF(%d): %s", (int) thread_num, message);
         }
         prof_reset(level, nullptr);

         sge_free_saved_vars(context);
         ret = true;
      }
   }

   DRETURN(ret);
}


/**
 * @brief Initialize the sge_prof_info_t struc array with default values
 *
 * initialize the sge_prof_info_t struct array with default values
 *
 * @return initialized sge_prof_info_t array for the given profiling level
 *
 * @note MT-NOTE: prof_info_init() is MT safe
 */
static void prof_info_init(prof_level level, pthread_t thread_id) {
   int thread_num;

   thread_num = get_prof_info_thread_id(thread_id);

   if (level <= SGE_PROF_ALL) {
      if (level == SGE_PROF_ALL) {
         int i;
         for (i = SGE_PROF_OTHER; i <= SGE_PROF_ALL; i++) {
            prof_info_level_init((prof_level)i, thread_num);
         }
      } else {
         prof_info_level_init(SGE_PROF_OTHER, thread_num);
      }

      theInfo[thread_num][SGE_PROF_ALL].akt_level = SGE_PROF_NONE;
   }
}

/**
 * @brief Initialize the sge_prof_info_t struc array with default values
 *
 * initialize the sge_prof_info_t struct array with default values
 *
 * @return initialized sge_prof_info_t array for the given profiling level
 *
 * @note MT-NOTE: prof_info_level_init() is MT safe
 */
static void prof_info_level_init(prof_level i, int thread_num) {
   switch (i) {
      case SGE_PROF_OTHER:
         theInfo[thread_num][i].name = "other";
         break;
      case SGE_PROF_COMMUNICATION:
         theInfo[thread_num][i].name = "communication";
         break;
      case SGE_PROF_PACKING:
         theInfo[thread_num][i].name = "packing";
         break;
      case SGE_PROF_EVENTCLIENT:
         theInfo[thread_num][i].name = "eventclient";
         break;
      case SGE_PROF_EVENTMASTER:
         theInfo[thread_num][i].name = "eventmaster";
         break;
      case SGE_PROF_MIRROR:
         theInfo[thread_num][i].name = "mirror";
         break;
      case SGE_PROF_SPOOLING:
         theInfo[thread_num][i].name = "spooling";
         break;
      case SGE_PROF_SPOOLINGIO:
         theInfo[thread_num][i].name = "spooling-io";
         break;
      case SGE_PROF_JOBSCRIPT:
         theInfo[thread_num][i].name = "spooling-script";
         break;
      case SGE_PROF_GDI:
         theInfo[thread_num][i].name = "gdi";
         break;
      case SGE_PROF_GDI_REQUEST:
         theInfo[thread_num][i].name = "gdi_request";
         break;
      case SGE_PROF_HT_RESIZE:
         theInfo[thread_num][i].name = "ht-resize";
         break;
      case SGE_PROF_ALL:
         theInfo[thread_num][i].name = "all";
         break;
      default:
         theInfo[thread_num][i].name = nullptr; /* "custom"*/
         break;
   }

   theInfo[thread_num][i].nested_calls = 0;
   theInfo[thread_num][i].start = 0;
   theInfo[thread_num][i].end = 0;
   theInfo[thread_num][i].tms_start.tms_utime = 0;
   theInfo[thread_num][i].tms_start.tms_stime = 0;
   theInfo[thread_num][i].tms_start.tms_cutime = 0;
   theInfo[thread_num][i].tms_start.tms_cstime = 0;
   theInfo[thread_num][i].tms_end.tms_utime = 0;
   theInfo[thread_num][i].tms_end.tms_stime = 0;
   theInfo[thread_num][i].tms_end.tms_cutime = 0;
   theInfo[thread_num][i].tms_end.tms_cstime = 0;
   theInfo[thread_num][i].total = 0;
   theInfo[thread_num][i].total_utime = 0;
   theInfo[thread_num][i].total_stime = 0;

   theInfo[thread_num][i].pre = SGE_PROF_NONE;
   theInfo[thread_num][i].sub = 0;
   theInfo[thread_num][i].sub_utime = 0;
   theInfo[thread_num][i].sub_stime = 0;
   theInfo[thread_num][i].sub_total = 0;
   theInfo[thread_num][i].sub_total_utime = 0;
   theInfo[thread_num][i].sub_total_stime = 0;

   theInfo[thread_num][i].prof_is_started = false;
   theInfo[thread_num][i].start_clock = 0;
   theInfo[thread_num][i].ever_started = false;

   theInfo[thread_num][i].info_string.s = nullptr;
   theInfo[thread_num][i].info_string.length = 0;
   theInfo[thread_num][i].info_string.size = 0;
   theInfo[thread_num][i].info_string.is_static = false;

}

/**
 * @brief Mallocs memory for the sge_prof_info_t array
 *
 * mallocs memory for sge_prof_info_t array for the number
 * of MAX_THREAD_NUM threads
 * mallocs memory for each thread if nedded
 *
 * @note MT-NOTE: init_array() is MT safe
 */
static void init_array(pthread_t num) {

   int i, c;

   DENTER(CULL_LAYER);

   if (sge_prof_array_initialized == 0) {
      CRITICAL("Profiling array is not initialized!\n");
      ocs::TerminationManager::trigger_abort();
   }

   pthread_mutex_lock(&thrdInfo_mutex);

   for (i = 0; i < MAX_THREAD_NUM; i++) {
      if (theInfo[i] != nullptr && theInfo[i][SGE_PROF_ALL].thread_id == num) {
         break;
      } else if (theInfo[i] == nullptr) {

         theInfo[i] = (sge_prof_info_t *) sge_malloc((SGE_PROF_ALL + 1) * sizeof(sge_prof_info_t));
         SGE_ASSERT(theInfo[i] != nullptr);
         memset(theInfo[i], 0, (SGE_PROF_ALL + 1) * sizeof(sge_prof_info_t));

         for (c = 0; c <= SGE_PROF_ALL; c++) {
            theInfo[i][c].thread_id = num;
         }

         /* Rather than malloc'ing space for an int that we'll have to clean up
          * later, we just store the int directly in the thread local storage.
          * In order to keep Linux from complaining, the value we store must be
          * the same size as a void pointer, i.e. a long. */
         long storage = i;
         pthread_setspecific(thread_id_key, (void *) storage);

         prof_info_init(SGE_PROF_ALL, num);

         break;
      }
   }

   pthread_mutex_unlock(&thrdInfo_mutex);

   DRETURN_VOID;

}

/* per process initialization */
static void init_array_first() {

   if (sge_prof_array_initialized == 0) {
      pthread_mutex_lock(&thrdInfo_mutex);

      if (pthread_key_create(&thread_id_key, nullptr) == 0) {
         theInfo = (sge_prof_info_t **) sge_malloc(MAX_THREAD_NUM * sizeof(sge_prof_info_t *));
         memset(theInfo, 0, MAX_THREAD_NUM * sizeof(sge_prof_info_t *));
         sge_prof_array_initialized = 1;
      }

      pthread_mutex_unlock(&thrdInfo_mutex);
   }

}

/**
 * @brief Mallocs memory for the thread_info_t array
 *
 * mallocs memory for thread_info_t array (thread name/id mapping)
 * for the number of MAX_THREAD_NUM threads. Must be called once
 * per process.
 *
 * @note MT-NOTE: init_thread_info() is MT safe
 */
static void init_thread_info() {

   if (!profiling_enabled) {
      return;
   }

   pthread_mutex_lock(&thrdInfo_mutex);

   if (thrdInfo == nullptr) {
      thrdInfo = (sge_thread_info_t *) sge_malloc(MAX_THREAD_NUM * sizeof(sge_thread_info_t));
      memset(thrdInfo, 0, MAX_THREAD_NUM * sizeof(sge_thread_info_t));
   }

   pthread_mutex_unlock(&thrdInfo_mutex);

}

/**
 * @brief Set the thread name mapped to its id
 *
 * maps the name and the id of a thread
 * set the thread profiling status to false
 *
 * @note MT-NOTE: set_thread_info() is MT safe
 *
 * @param thread_id id of the thread to name
 * @param thread_name the name to record
*/
void set_thread_name(pthread_t thread_id, const char *thread_name) {

   int thread_num;

   if (!profiling_enabled) {
      return;
   }

   init_thread_info();

   init_array(thread_id);

   thread_num = get_prof_info_thread_id(thread_id);

   if ((thread_num >= 0) && (thread_num < MAX_THREAD_NUM)) {
      pthread_mutex_lock(&thrdInfo_mutex);

      thrdInfo[thread_num].thrd_id = thread_id;
      thrdInfo[thread_num].thrd_name = thread_name;
      thrdInfo[thread_num].prof_is_active = false;
      thrdInfo[thread_num].is_terminated = 0;

      pthread_mutex_unlock(&thrdInfo_mutex);
   }
}


/**
 * @brief Sets the profiling status for the thread
 *
 * set the thread profiling status of the thread with the given id
 *
 * @note MT-NOTE: set_thread_prof_status_by_id() is MT safe
 *
 * @param thread_id id of the thread
 * @param prof_status true to make the thread profile
*/
void set_thread_prof_status_by_id(pthread_t thread_id, bool prof_status) {

   int thread_num;

   if (!profiling_enabled) {
      return;
   }

   init_thread_info();

   thread_num = get_prof_info_thread_id(thread_id);

   pthread_mutex_lock(&thrdInfo_mutex);

   if (thrdInfo[thread_num].thrd_id == thread_id) {
      thrdInfo[thread_num].prof_is_active = prof_status;
   }

   pthread_mutex_unlock(&thrdInfo_mutex);
}


/**
 * @brief Sets the profiling status for the thread
 *
 * set the thread profiling status of the thread with the given id and name
 *
 * @return ok return 1 - thread_name = nullptr
 *
 * @note MT-NOTE: set_thread_prof_status_by_name() is MT safe
 *
 * @param thread_name name of the thread
 * @param prof_status true to make the thread profile
*/
int set_thread_prof_status_by_name(const char *thread_name, bool prof_status) {

   int i;

   if (!profiling_enabled) {
      return 0;
   } else if (thread_name == nullptr) {
      return 1;
   }

   init_thread_info();

   pthread_mutex_lock(&thrdInfo_mutex);

   for (i = 0; i < MAX_THREAD_NUM; i++) {
      if (thrdInfo[i].thrd_name != nullptr) {
         if (strcmp(thrdInfo[i].thrd_name, thread_name) == 0) {
            thrdInfo[i].prof_is_active = prof_status;
         }
      }
   }

   pthread_mutex_unlock(&thrdInfo_mutex);

   return 0;
}

/** @brief cleanup function for profiling, to free allocated memory
 *
 * @IMPOTANT: Do not call directly. will be called automatically after main() returns (in ProfThreadInit
 */
static void sge_prof_cleanup() {

   if (!profiling_enabled) {
      return;
   }

   pthread_mutex_lock(&thrdInfo_mutex);

   pthread_key_delete(thread_id_key);

   if (theInfo != nullptr) {
      int c, i;

      for (c = 0; c < MAX_THREAD_NUM; c++) {
         for (i = 0; i <= SGE_PROF_ALL; i++) {
            if (theInfo[c] != nullptr) {
               sge_dstring_free(&theInfo[c][i].info_string);
            }
         }
         sge_free(&(theInfo[c]));
      }
      sge_free(&theInfo);
   }
   sge_free(&thrdInfo);

   sge_prof_array_initialized = 0;

   pthread_mutex_unlock(&thrdInfo_mutex);

}

/**
 * @brief Returns the status of a thread
 *
 * returns the profiling status of a thread
 *
 * @note MT-NOTE: thread_prof_active_by_id() is MT safe
 *
 * @param thread_id id of the thread
 * @return true when this thread collects profiling data
*/
bool thread_prof_active_by_id(pthread_t thread_id) {

   int thread_num;
   bool ret = false;

   if (profiling_enabled) {
      init_thread_info();

      thread_num = get_prof_info_thread_id(thread_id);

      if ((thread_num >= 0) && (thread_num < MAX_THREAD_NUM)) {
         pthread_mutex_lock(&thrdInfo_mutex);

         ret = thrdInfo[thread_num].prof_is_active;

         pthread_mutex_unlock(&thrdInfo_mutex);
      }
   }

   return ret;

}


/**
 * @brief Returns the status of a thread
 *
 * returns the profiling status of a thread
 *
 * @note MT-NOTE: thread_prof_active_by_name() is MT safe
 *
 * @param thread_name name of the thread
 * @return true when this thread collects profiling data
*/
bool thread_prof_active_by_name(const char *thread_name) {

   bool ret = false;

   if (profiling_enabled && (thread_name != nullptr)) {
      int c;

      init_thread_info();

      pthread_mutex_lock(&thrdInfo_mutex);

      for (c = 0; c < MAX_THREAD_NUM; c++) {
         if (thrdInfo[c].thrd_name != nullptr && strstr(thrdInfo[c].thrd_name, thread_name)) {
            ret = thrdInfo[c].prof_is_active;
            break;
         }
      }

      pthread_mutex_unlock(&thrdInfo_mutex);
   }

   return ret;
}


static int get_prof_info_thread_id(pthread_t thread_num) {
   long ret = -1;

   ret = (long) pthread_getspecific(thread_id_key);

   return (int) ret;
}

/**
 * @brief Start profiling for thread
 *
 * Checks if profiling has been enabled for the current thread.
 * If yes, starts profiling for all levels.
 * If profiling has been disabled for the current thread, profiling
 * is disabled for all levels.
 *
 * @note MT-NOTE: thread_start_stop_profiling() is MT safe
 */
void
thread_start_stop_profiling() {
   if (!profiling_enabled) {
      return;
   }

   if (thread_prof_active_by_id(pthread_self())) {
      prof_start(SGE_PROF_ALL, nullptr);
   } else {
      prof_stop(SGE_PROF_ALL, nullptr);
   }
}

/**
 * @brief Output profiling info for thread
 *
 * Outputs profiling information for the current thread.
 * Information for all active profiling levels is dumped.
 * The first line dumped is a sort of title, that can/should be used
 * to identify the thread.
 *
 * The variable next_prof_output will be set by this function.
 * This variable should be initialized to 0 (zero) by the caller before the
 * first call of this function.
 *
 * @param title title to print as first line
 * @param next_prof_output time of next profiling output
 *
 * @note MT-NOTE: thread_output_profiling() is MT safe
 *
 * @see #prof_output_info
 */
void
thread_output_profiling(const char *title, uint64_t *next_prof_output) {
   if (prof_is_active(SGE_PROF_ALL)) {
      uint64_t now = sge_get_gmt64();

      if (*next_prof_output == 0) {
         unsigned int seed = (unsigned int)(unsigned long)(pthread_self());
         *next_prof_output = now + sge_gmt32_to_gmt64(rand_r(&seed) % 20);
      } else {
         if (now >= *next_prof_output) {
            prof_output_info(SGE_PROF_ALL, false, title);
            *next_prof_output = now + sge_gmt32_to_gmt64(60);
         }
      }
   }
}
