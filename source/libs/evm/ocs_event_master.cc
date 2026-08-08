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
 * @brief The unique event id counter
 *
 * A plain atomic, so any thread may produce an event without taking a lock.
 *
 * @see ocs_event_master.h
 */

#include <atomic>

#include "ocs_event_master.h"

/**
 * @brief Hand out the next unique event id
 *
 * A plain atomic counter, so any thread may produce an event without taking a
 * lock. Ids are never reused within a qmaster lifetime.
 *
 * @return the next id
 */
uint64_t
oge_get_next_unique_event_id() {
   static std::atomic_uint_fast64_t id = 0;
   return atomic_fetch_add(&id, 1);
}

