#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2024 HPC-Gridware GmbH
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

#ifdef WITH_GPERF
#  include <gperftools/profiler.h>

typedef struct {
   std::string gperf_name;
   bool gperf_started;
} sge_gperf_per_thread_t;

extern bool g_scheduler_use_gperftools;

void
sge_gperf_per_thread_init(sge_gperf_per_thread_t &per_thread_data);

bool
sge_gperf_start_profiling(sge_gperf_per_thread_t &per_thread_data, const std::string &thread_name, const std::string &thread_pattern, const std::string &gperf_name);

bool
sge_gperf_stop_profiling(sge_gperf_per_thread_t &per_thread_data, const std::string &thread_name);

bool
sge_gperf_do_profiling(sge_gperf_per_thread_t &per_thread_data, const std::string &thread_name, const std::string &threads, const std::string &gperf_name);

#endif
