#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2023-2024,2026 HPC-Gridware GmbH
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
 * @brief Fine grained locking: many small locks taken in a safe order
 *
 * Instead of one lock per list, a thread can lock individual elements. Each
 * lock is named by a root id (the list) and optionally an element id, either
 * numeric (#fgl_add_u) or a string (#fgl_add_s); #fgl_add_r locks the list
 * itself.
 *
 * Deadlock is avoided by ordering, not by a hierarchy: a thread first
 * *registers* every lock it will need, and #fgl_lock then sorts the requests
 * into one canonical order before acquiring any of them. Two threads wanting
 * the same set therefore always take them in the same sequence.
 *
 * The usage is register, lock, work, unlock:
 *
 * @code
 * fgl_add_r(SGE_TYPE_JOB, true);
 * fgl_add_u(SGE_TYPE_JOB, job_id, true);
 * fgl_lock();
 * // ... work ...
 * fgl_unlock();
 * @endcode
 *
 * #fgl_unlock also clears the request list, so the next round starts empty.
 * The requests are thread-local; the locks themselves are process-wide.
 */

void fgl_rsv_sort();

void fgl_add_r(uint32_t root, bool rw);

void fgl_add_u(uint32_t root, uint32_t id, bool rw);

void fgl_add_s(uint32_t root, const char *id, bool rw);

void fgl_clear();

void fgl_dump(dstring *dstr);

void fgl_dump_stats(dstring *stats_str);

void fgl_lock();

void fgl_unlock();
