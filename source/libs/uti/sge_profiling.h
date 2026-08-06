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
 * @brief Runtime profiling: measure and report where time is spent
 */

#include <ctime>
#include <sys/times.h>
#include <pthread.h>

#if defined(FREEBSD)
#include <pthread.h>
#endif

#include <cinttypes>

#include "sge_dstring.h"

/** @defgroup uti_profiling Profiling
 * @brief Measure and report where a program spends its time
 *
 * Reports wallclock, busy time, and user and system CPU time. Profiling can be
 * started, stopped and reset, and individual code blocks measured by starting
 * and stopping a measurement around them. Results can be read back one metric
 * at a time, or as a ready formatted summary with `prof_get_info_string()`.
 *
 * @section uti_profiling_levels Levels
 *
 * Measurements are attributed to a level, so time spent in communication,
 * spooling and so on can be told apart. Besides the predefined levels there are
 * ten custom ones, `SGE_PROF_CUSTOM0` to `SGE_PROF_CUSTOM9`, free for any use.
 *
 * The predefined levels belong to the component they are named after and should
 * only be used by that component. `SGE_PROF_OTHER` is maintained by this module
 * and collects whatever the other levels do not cover - do not use it from
 * outside.
 *
 * Measurements nest: starting one while another is still running attributes the
 * inner measurement to the outer level as well, and when reading results a
 * parameter decides whether nested measurements are included. Starting
 * measurements cyclically is not allowed and switches profiling off.
 *
 * @note MT-NOTE: this module is MT safe provided `prof_mt_init()` and/or
 *       #sge_prof_set_enabled are called before profiling starts, and
 *       `sge_prof_cleanup()` is called after every thread that profiles has
 *       stopped. `sge_prof_cleanup()` frees the profiling array.
 *
 * @bug In a multithreaded program the `times()` system call reports per process
 *      CPU times, but this module treats them as per thread values. The CPU
 *      figures of a single thread are therefore too high whenever other threads
 *      are busy at the same time.
 * @{
 */

/** @brief The area a measurement is attributed to, see @ref uti_profiling_levels */
typedef enum {
   SGE_PROF_NONE = -1,       ///< no level, used where a level argument is optional
   SGE_PROF_OTHER = 0,       ///< time not attributed to any other level; maintained by this module, do not use directly
   SGE_PROF_COMMUNICATION,   ///< sending and receiving over the commlib
   SGE_PROF_PACKING,         ///< packing and unpacking of messages
   SGE_PROF_EVENTCLIENT,     ///< event client side processing
   SGE_PROF_EVENTMASTER,     ///< event master side processing
   SGE_PROF_MIRROR,          ///< the event mirror
   SGE_PROF_SPOOLING,        ///< spooling, excluding the raw IO
   SGE_PROF_SPOOLINGIO,      ///< the raw IO performed while spooling
   SGE_PROF_JOBSCRIPT,       ///< writing and reading job scripts
   SGE_PROF_GDI,             ///< GDI processing
   SGE_PROF_GDI_REQUEST,     ///< one individual GDI request
   SGE_PROF_HT_RESIZE,       ///< hash table resizing
   SGE_PROF_SCHEDULER,       ///< a scheduling run
   SGE_PROF_CUSTOM0,         ///< free for any use by any component
   SGE_PROF_CUSTOM1,         ///< free for any use by any component
   SGE_PROF_CUSTOM2,         ///< free for any use by any component
   SGE_PROF_CUSTOM3,         ///< free for any use by any component
   SGE_PROF_CUSTOM4,         ///< free for any use by any component
   SGE_PROF_CUSTOM5,         ///< free for any use by any component
   SGE_PROF_CUSTOM6,         ///< free for any use by any component
   SGE_PROF_CUSTOM7,         ///< free for any use by any component
   SGE_PROF_CUSTOM8,         ///< free for any use by any component
   SGE_PROF_CUSTOM9,         ///< free for any use by any component
   SGE_PROF_SCHEDLIB0,       ///< free for use inside the scheduler library
   SGE_PROF_SCHEDLIB1,       ///< free for use inside the scheduler library
   SGE_PROF_SCHEDLIB2,       ///< free for use inside the scheduler library
   SGE_PROF_SCHEDLIB3,       ///< free for use inside the scheduler library
   SGE_PROF_SCHEDLIB4,       ///< used by `sgeee.cc`; deliberately left out of the scheduler overview
   SGE_PROF_ALL              ///< every level, for calls that operate on all of them
} prof_level;

/** @brief Accumulated profiling data of one level in one thread
 *
 * One of these per level per thread. The `sub_*` fields hold the share spent in
 * nested measurements, which is what makes it possible to report a level with
 * or without its sub measurements.
 */
typedef struct {
   const char *name;          ///< display name of the level
   int nested_calls;          ///< depth of nested measurements in this level
   clock_t start;             ///< start of the current measurement
   clock_t end;               ///< end of the last measurement
   struct tms tms_start;      ///< CPU times at the start of the current measurement
   struct tms tms_end;        ///< CPU times at the end of the last measurement
   clock_t total;             ///< clock ticks summed over all measurements
   clock_t total_utime;       ///< user CPU time summed over all measurements
   clock_t total_stime;       ///< system CPU time summed over all measurements

   prof_level pre;            ///< level this one is nested in, #SGE_PROF_ALL when none
   clock_t sub;               ///< time spent in nested measurements
   clock_t sub_utime;         ///< user CPU time spent in nested measurements
   clock_t sub_stime;         ///< system CPU time spent in nested measurements
   clock_t sub_total;         ///< #sub summed over all measurements
   clock_t sub_total_utime;   ///< #sub_utime summed over all measurements
   clock_t sub_total_stime;   ///< #sub_stime summed over all measurements

   bool prof_is_started;      ///< true while profiling runs for this level
   clock_t start_clock;       ///< clock value when profiling was started
   prof_level akt_level;      ///< level currently being measured
   bool ever_started;         ///< true once profiling has been started at least once
   pthread_t thread_id;       ///< thread this data belongs to
   dstring info_string;       ///< buffer for `prof_get_info_string()`
} sge_prof_info_t;

void sge_prof_set_enabled(bool enabled);

bool thread_prof_active_by_id(pthread_t thread_id);

bool thread_prof_active_by_name(const char *thread_name);

void set_thread_name(pthread_t thread_id, const char *thread_name);

void set_thread_prof_status_by_id(pthread_t thread_id, bool prof_status);

int set_thread_prof_status_by_name(const char *thread_name, bool prof_status);

bool prof_set_level_name(prof_level level, const char *name, dstring *error);

bool prof_is_active(prof_level level);

bool prof_start(prof_level level, dstring *error);

bool prof_stop(prof_level level, dstring *error);

void
prof_start_stop(prof_level level, dstring *error, bool do_start);

bool prof_start_measurement(prof_level level, dstring *error);

bool prof_stop_measurement(prof_level level, dstring *error);

/**
 * @brief Starts the measurement for the specified level
 *
 * starts the measurement for the specified level
 * to use profiling the sge_prof_setup() function
 * must be called in the main program first.
 *
 * @code
 * PROF_START_MEASUREMENT(SGE_PROF_GDI)
 * @endcode
 *
 * @note MT-NOTE: PROF_START_MEASUREMENT() is MT safe
 */
/// start a measurement for @p level, if profiling is active
#define PROF_START_MEASUREMENT(level) \
   if(prof_is_active(level)) {\
      prof_start_measurement(level,nullptr);\
   }

/**
 * @brief Stops the measurement for the specified level
 *
 * stops the measurement for the specified level
 * to use profiling the sge_prof_setup() function
 * must be called in the main program first.
 *
 * @code
 * PROF_STOP_MEASUREMENT(SGE_PROF_GDI)
 * @endcode
 *
 * @note MT-NOTE: PROF_STOP_MEASUREMENT() is MT safe
 */
/// stop the measurement for @p level, if profiling is active
#define PROF_STOP_MEASUREMENT(level) \
   if(prof_is_active(level)) {\
      prof_stop_measurement(level, nullptr);\
   }

bool prof_reset(prof_level level, dstring *error);

double prof_get_measurement_wallclock(prof_level level, bool with_sub, dstring *error);

double prof_get_measurement_utime(prof_level level, bool with_sub, dstring *error);

double prof_get_measurement_stime(prof_level level, bool with_sub, dstring *error);

double prof_get_total_wallclock(prof_level level, dstring *error);

double prof_get_total_busy(prof_level level, bool with_sub, dstring *error);

double prof_get_total_utime(prof_level level, bool with_sub, dstring *error);

double prof_get_total_stime(prof_level level, bool with_sub, dstring *error);

const char *prof_get_info_string(prof_level level, bool with_sub, dstring *error);

bool prof_output_info(prof_level level, bool with_sub, const char *info);

void thread_start_stop_profiling();

void thread_output_profiling(const char *title, uint64_t *next_prof_output);

/** @} */
