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

#include "uti/sge_rmon_macros.h"
#include "uti/sge_component.h"

#include "cull/cull_list.h"
#include "cull/cull_multitype.h"

#include "sgeobj/sge_str.h"

#include "sge_thread_utility.h"

#include <sge_rmon_monitoring_level.h>
#include <sge_thread_main.h>
#include <sge_thread_scheduler.h>

static void
add_active_threads(cl_raw_list_t *threads, lList *active_thread_list) {
   cl_thread_list_elem_t *thiz;
   cl_thread_list_elem_t *next = cl_thread_list_get_first_elem(threads);

   // Count the number of threads in the thread pool
   const char *name = next->thread_config->thread_name;
   int nthreads = 0;
   while ((thiz = next) != nullptr) {
      next = cl_thread_list_get_next_elem(thiz);
      nthreads++;
   }

   // one entry per thread pool with the number of threads
   lListElem *st_elem = lAddElemStr(&active_thread_list, ST_name, name, ST_Type);
   lSetUlong(st_elem, ST_id, nthreads);
}


lList *
get_active_thread_list() {
   DENTER(TOP_LAYER);
   lList *active_thread_list = lCreateList("", ST_Type);

   // Internal threads where the user has no influence on, will not be shown
#if 0
   // static threads that cannot be disabled
   lAddElemStr(&active_thread_list, ST_name, threadnames[SIGNAL_THREAD], ST_Type);
   lAddElemStr(&active_thread_list, ST_name, threadnames[TIMER_THREAD], ST_Type);
   lAddElemStr(&active_thread_list, ST_name, threadnames[EVENT_MASTER_THREAD], ST_Type);

   // Mirror threads
   add_active_threads(Main_Control.mirror_thread_pool, active_thread_list);

   // Commlib threads
   add_active_threads(cl_com_get_thread_list(), active_thread_list);
#endif

   // Scheduler thread if it is running
   if (sge_scheduler_is_running()) {
      lListElem *st_elem = lAddElemStr(&active_thread_list, ST_name, threadnames[SCHEDD_THREAD], ST_Type);
      lSetUlong(st_elem, ST_id, 1);
   }

   // Thread pools with dynamic number of threads
   add_active_threads(Main_Control.listener_thread_pool, active_thread_list);
   add_active_threads(Main_Control.reader_thread_pool, active_thread_list);
   add_active_threads(Main_Control.worker_thread_pool, active_thread_list);

   // Sort the list
   lPSortList(active_thread_list, "%I+", ST_name);

   DRETURN(active_thread_list);
}
