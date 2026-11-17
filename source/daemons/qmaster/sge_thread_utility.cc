/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024,2026 HPC-Gridware GmbH
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

/** @file
 * @brief Helpers shared by the qmaster threads
 */

#include "uti/sge_rmon_macros.h"
#include "uti/sge_component.h"

#include "cull/cull_list.h"
#include "cull/cull_multitype.h"

#include "sgeobj/sge_str.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Sort.h"

#include "sge_thread_utility.h"

#include <sge_rmon_monitoring_level.h>
#include <sge_thread_main.h>
#include <sge_thread_scheduler.h>

/**
 * @brief Add one entry for a thread pool to the list of active threads
 *
 * @param threads the commlib thread list of the pool, holding at least one thread
 * @param active_thread_list the list to add to (ST_Type); `ST_name` gets the name of the pool's first thread,
 *                           `ST_id` the number of threads in the pool
 */
static void
add_active_threads(cl_raw_list_t *threads, const ocs::tom::List<ocs::tom::ST> active_thread_list) {
   DENTER(TOP_LAYER);

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
   const auto st_elem = active_thread_list.emplace_back<ST_name>(name);
   ocs::tom::set<ST_id>(st_elem, nthreads);

   DRETURN_VOID;
}


/** @brief The names of the running thread pools and how many threads each has
 *
 * Only the threads whose number an administrator can influence are reported:
 * the scheduler, and the listener, reader and worker pools. The signal, timer,
 * event master, mirror and commlib threads are deliberately left out (the code
 * for them is kept but disabled) because their number is not configurable and
 * listing them would suggest otherwise.
 *
 * @return a newly created `ST_Type` list, one element per pool, `ST_name` the
 *         pool name and `ST_id` the number of threads in it, sorted by name.
 */
ocs::tom::ListPtr<ocs::tom::ST>
ocs_get_active_thread_list() {
   DENTER(TOP_LAYER);
   auto active_thread_list = ocs::tom::ListPtr<ocs::tom::ST>::create("");

   // Internal threads where the user has no influence on, will not be shown
#if 0
   // static threads that cannot be disabled
   lAddElemStr(&active_thread_list, ST_name, to_cstr(SIGNAL_THREAD), ST_Type);
   lAddElemStr(&active_thread_list, ST_name, to_cstr(TIMER_THREAD), ST_Type);
   lAddElemStr(&active_thread_list, ST_name, to_cstr(EVENT_MASTER_THREAD), ST_Type);

   // Mirror threads
   add_active_threads(Main_Control.mirror_thread_pool, active_thread_list);

   // Commlib threads
   add_active_threads(cl_com_get_thread_list(), active_thread_list);
#endif

   // Scheduler thread if it is running
   if (sge_scheduler_is_running()) {
      const auto st_elem = active_thread_list->emplace_back<ST_name>(to_cstr(SCHEDD_THREAD));
      ocs::tom::set<ST_id>(st_elem, 1);
   }

   // Thread pools with dynamic number of threads
   add_active_threads(Main_Control.listener_thread_pool, active_thread_list.get());
   add_active_threads(Main_Control.reader_thread_pool, active_thread_list.get());
   add_active_threads(Main_Control.worker_thread_pool, active_thread_list.get());

   // Sort the list
   active_thread_list->sort<ocs::tom::asc<ST_name>>();

   DRETURN(active_thread_list);
}

/**
 * @brief ocs_get_active_thread_list() for an untyped caller
 *
 * The GDI request data and the thread pool provider of the qconf procedures still take a raw list.
 *
 * @return as ocs_get_active_thread_list(); the caller owns the list
 */
lList *
get_active_thread_list() {
   return ocs_get_active_thread_list().release();
}
