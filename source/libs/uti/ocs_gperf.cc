/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
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
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include <string>
#include <fnmatch.h>

#include "uti/ocs_gperf.h"

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include <sge_log.h>

#ifdef WITH_GPERF

#include <gperftools/profiler.h>

bool g_scheduler_use_gperftools = false;

void
sge_gperf_per_thread_init(sge_gperf_per_thread_t &data) {
   data.gperf_name = "";
   data.gperf_started = false;
}

bool
sge_gperf_start_profiling(sge_gperf_per_thread_t &per_thread_data, const std::string &thread_name, const std::string &thread_pattern, const std::string &gperf_name) {
   DENTER(TOP_LAYER);

   bool ret = fnmatch(thread_pattern.c_str(), thread_name.c_str(), 0) == 0 ? true : false;
   if (ret) {
      sge_setenv("HEAP_PROFILE_ALLOCATION_INTERVAL", "104857600");
      sge_setenv("HEAP_PROFILE_INUSE_INTERVAL", "104857600");
      sge_setenv("CPUPROFILE_PER_THREAD_TIMERS", "t");
      sge_setenv("HEAP_PROFILE_ONLY_MMAP", "true");

      sigset_t sigset;
      sigemptyset(&sigset);
      sigaddset(&sigset, SIGPROF);
      sigprocmask(SIG_UNBLOCK, &sigset, nullptr);

      if (gperf_name != per_thread_data.gperf_name) {
         per_thread_data.gperf_name = gperf_name;
      }

      std::string filename = "/tmp/" + thread_name + "-" + std::to_string(sge_get_gmt64())+ "-" + gperf_name;

      if (per_thread_data.gperf_started) {
         ERROR("Profiling already started");
         ProfilerFlush();
         ProfilerStop();
         per_thread_data.gperf_started = false;
      }
      if (!per_thread_data.gperf_started) {
         INFO("Starting profiling %s", filename.c_str());
         ProfilerStart(filename.c_str());
         ProfilerRegisterThread();
         per_thread_data.gperf_started = true;
      }
   } else {
      // Profiling has been disabled for this thread? Then stop it if it was active.
      if (per_thread_data.gperf_started) {
         INFO("Stopping profiling");
         ProfilerFlush();
         ProfilerStop();
         per_thread_data.gperf_started = false;
      }
   }

   DRETURN(ret);
}

bool
sge_gperf_stop_profiling(sge_gperf_per_thread_t &per_thread_data, const std::string &thread_name) {
   DENTER(TOP_LAYER);

   if (per_thread_data.gperf_started) {
      INFO("Stopping profiling");
      ProfilerFlush();
      ProfilerStop();
      per_thread_data.gperf_started = false;
   }

   DRETURN(true);
}

bool
sge_gperf_do_profiling(sge_gperf_per_thread_t &per_thread_data, const std::string &thread_name, const std::string &thread_pattern, const std::string &gperf_name) {
   DENTER(TOP_LAYER);

   const bool ret = fnmatch(thread_pattern.c_str(), thread_name.c_str(), 0) == 0 ? true : false;
   if (ret) {
      sge_setenv("HEAP_PROFILE_ALLOCATION_INTERVAL", "104857600");
      sge_setenv("HEAP_PROFILE_INUSE_INTERVAL", "104857600");
      sge_setenv("CPUPROFILE_PER_THREAD_TIMERS", "t");
      sge_setenv("HEAP_PROFILE_ONLY_MMAP", "true");

      // PROFILESELECTED=1 -- 
      // if set, cpu-profiler will only profile regions of code
      // surrounded with ProfilerEnable()/ProfilerDisable().
      sigset_t sigset;
      sigemptyset(&sigset);
      sigaddset(&sigset, SIGPROF);
      sigprocmask(SIG_UNBLOCK, &sigset, nullptr);

      // start/stop only change profile name changes
      if (gperf_name == per_thread_data.gperf_name) {
         per_thread_data.gperf_name = gperf_name;
         std::string filename = "/tmp/" + thread_name + "-" + std::to_string(sge_get_gmt64())+ "-" + gperf_name;

         if (per_thread_data.gperf_started) {
            ProfilerFlush();
            ProfilerStop();
            per_thread_data.gperf_started = false;
         }
         if (!per_thread_data.gperf_started) {
            ProfilerStart(filename.c_str());
            ProfilerRegisterThread();
            per_thread_data.gperf_started = true;
         }
      } else {
         ProfilerFlush();
      }
   } else {
      if (per_thread_data.gperf_started) {
         ProfilerFlush();
         ProfilerStop();
         per_thread_data.gperf_started = false;
      }
   }

   DRETURN(ret);
}

#endif
