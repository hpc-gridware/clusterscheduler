/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * @brief Implementation of the fine grained locking described in sge_fgl.h
 *
 * A process-wide hash table maps each lock key to a read/write lock and a
 * reference count; a thread-local array holds the requests registered so far.
 * #fgl_lock sorts that array with #fgl_rsv_compare before acquiring anything,
 * which is what makes the scheme deadlock free.
 */

#include <pthread.h>
#include <cstdio>
#include <cstring>
#include <cctype>

#include <cinttypes>
#include "uti/sge_dstring.h"
#include "uti/sge_fgl.h"
#include "uti/sge_htable.h"
#include "uti/sge_lock_fifo.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_mtutil.h"

#define COLLECT_STATS 1        ///< set to 0 to drop the per-lock wait time statistics
#define USE_FIFO_LOCK 0        ///< 1 uses the FIFO lock: fair, but a memory eater and slower
#define LOCK_ALL_LISTS_FIRST 0 ///< 1 orders all list locks before any element lock; ~20% slower

#define FGL_REQ_MAX 128        ///< requests a thread may register before #fgl_lock; exceeding it aborts

/// What a lock request names, and therefore which member of fgl_t::u is valid
typedef enum {
   FGL_NONE,   ///< the list itself; no element id
   FGL_ULONG,  ///< an element identified by a number
   FGL_STR,    ///< an element identified by a string
} fgl_type_t;

/// One registered lock request
typedef struct {
   uint32_t id_root;   ///< the list the lock belongs to
   bool is_rw;         ///< true for a write lock, false for a read lock
   fgl_type_t type;    ///< which kind of element is named, and which union member is valid
   union {
      uint32_t id_ulong; ///< element id, when type is #FGL_ULONG
      const char *id_str;///< element id, when type is #FGL_STR; not copied, must outlive the request
   } u;                ///< the element id
} fgl_t;

/// One entry of the process-wide lock table
typedef struct {
#if USE_FIFO_LOCK
   sge_fifo_rw_lock_t lck; ///< the lock itself
#else
   pthread_rwlock_t lck;   ///< the lock itself
#endif
   uint32_t counter;       ///< how many threads currently hold or want it; the entry dies at 0
} fgl_lck_t;

/// The lock requests one thread has registered but not yet acquired
typedef struct {
   fgl_t requests[FGL_REQ_MAX]; ///< the requests, sorted in place by #fgl_rsv_sort
   uint32_t pos;                ///< number of requests registered so far
} fgl_state_t;

static pthread_once_t fgl_once = PTHREAD_ONCE_INIT;

static pthread_key_t fgl_state_key;

static htable fgl_lcks = nullptr;

static pthread_mutex_t fgl_mtx = PTHREAD_MUTEX_INITIALIZER;

#if COLLECT_STATS
/// Wait time statistics for one lock key, collected when #COLLECT_STATS is on
typedef struct {
   uint32_t qpos;          ///< position this key was acquired at within a sorted request set
   uint32_t measurements;  ///< how many acquisitions were timed
   double avg_wait_time;   ///< mean seconds spent waiting for this lock
   double min_wait_time;   ///< shortest wait seen
   double max_wait_time;   ///< longest wait seen
} fgl_stats_t;

static htable fgl_stats = nullptr;

static pthread_mutex_t fgl_stats_mtx = PTHREAD_MUTEX_INITIALIZER;

static dstring fgl_stats_str = DSTRING_INIT;
#endif


static void fgl_state_init(fgl_state_t *state) {
   memset(state, 0, sizeof(fgl_state_t));
}

static void fgl_state_destroy(void *st) {
   fgl_state_t *state = (fgl_state_t *)st;
   sge_free(&state);
}

static void fgl_once_init() {
   pthread_key_create(&fgl_state_key, fgl_state_destroy);
}

static void fgl_state_get_requests(fgl_t **requests, uint32_t *pos) {
   if (requests == nullptr || pos == nullptr) {
      return;
   }

   GET_SPECIFIC(fgl_state_t, fgl_state, fgl_state_init, fgl_state_key);
   *requests = fgl_state->requests;
   *pos = fgl_state->pos;
}

static void fgl_state_set_pos(uint32_t new_pos) {
   GET_SPECIFIC(fgl_state_t, fgl_state, fgl_state_init, fgl_state_key);
   fgl_state->pos = new_pos;
}

/**
 * @brief Initialise the module's thread-local key and lock tables
 *
 * Safe to call more than once; only the first call does anything.
 */
void fgl_mt_init() {
   pthread_once(&fgl_once, fgl_once_init);
   pthread_mutex_lock(&fgl_mtx);
   if (fgl_lcks == nullptr) {
      fgl_lcks = sge_htable_create(FGL_REQ_MAX, dup_func_string, hash_func_string, hash_compare_string);
   }
   pthread_mutex_unlock(&fgl_mtx);
#if COLLECT_STATS
   pthread_mutex_lock(&fgl_stats_mtx);
   if (fgl_stats == nullptr) {
      fgl_stats = sge_htable_create(FGL_REQ_MAX, dup_func_string, hash_func_string, hash_compare_string);
   }
   pthread_mutex_unlock(&fgl_stats_mtx);
#endif
}

/**
 * @brief Creates the thread-local key and the lock tables before `main()` runs
 *
 * Exists only for the side effect of its constructor. A single static instance
 * is defined below; do not remove it, and do not instantiate it anywhere else.
 */
class FeatureThreadInit {
public:
   /// Runs the one-time initialisation for this module
   FeatureThreadInit() {
      fgl_mt_init();
   }
};

// although not used the constructor call has the side effect to initialize the pthread_key => do not delete
static FeatureThreadInit feature_obj{};


/**
 * @brief Order two lock requests, giving every thread the same locking sequence
 *
 * This ordering is what makes the scheme deadlock free, so it must be a total
 * order over the requests. Lists and elements are ordered together by root id,
 * unless #LOCK_ALL_LISTS_FIRST is set, in which case all list locks sort before
 * any element lock.
 *
 * @param a first request, as a `fgl_t *`
 * @param b second request, as a `fgl_t *`
 * @return negative if @p a sorts first, positive if @p b does, 0 if equal
 */
int fgl_rsv_compare(const void *a, const void *b) {
   fgl_t *x = (fgl_t *) a;
   fgl_t *y = (fgl_t *) b;

#if LOCK_ALL_LISTS_FIRST
   // Lists first according to list ID then elements in the same sequence
   if (x->type == FGL_NONE && y->type == FGL_NONE) {
      if (x->id_root < y->id_root) {
         return -1;
      } else if (x->id_root > y->id_root) {
         return 1;
      } else {
         return 0;
      }
   } else if (x->type == FGL_NONE && y->type == FGL_ULONG) {
      return -1;
   } else if (x->type == FGL_NONE && y->type == FGL_STR) {
      return -1;
   } else if (x->type == FGL_ULONG && y->type == FGL_NONE) {
      return 1;
   } else if (x->type == FGL_ULONG && y->type == FGL_ULONG) {
      if (x->id_root < y->id_root) {
         return -1;
      } else if (x->id_root > y->id_root) {
         return 1;
      } else {
         if (x->u.id_ulong < y->u.id_ulong) {
            return -1;
         } else if (x->u.id_ulong > y->u.id_ulong) {
            return 1;
         } else {
            return 0;
         }
      }
   } else if (x->type == FGL_ULONG && y->type == FGL_STR) {
      return -1;
   } else if (x->type == FGL_STR && y->type == FGL_NONE) {
      return 1;
   } else if (x->type == FGL_STR && y->type == FGL_ULONG) {
      return 1;
   } else if (x->type == FGL_STR && y->type == FGL_STR) {
      if (x->id_root < y->id_root) {
         return -1;
      } else if (x->id_root > y->id_root) {
         return 1;
      } else {
         return strcmp(x->u.id_str, y->u.id_str);
      }
   }
#else
   // Lists and elements together sorted according to list ID
   if (x->id_root < y->id_root) {
      return -1;
   } else if (x->id_root > y->id_root) {
      return 1;
   } else {
      if (x->type == FGL_NONE && y->type == FGL_NONE) {
         return 0;
      } else if (x->type == FGL_NONE && y->type == FGL_ULONG) {
         return -1;
      } else if (x->type == FGL_NONE && y->type == FGL_STR) {
         return -1;
      } else if (x->type == FGL_ULONG && y->type == FGL_NONE) {
         return 1;
      } else if (x->type == FGL_ULONG && y->type == FGL_ULONG) {
         if (x->u.id_ulong < y->u.id_ulong) {
            return -1;
         } else if (x->u.id_ulong > y->u.id_ulong) {
            return 1;
         } else {
            return 0;
         }
      } else if (x->type == FGL_ULONG && y->type == FGL_STR) {
         return -1;
      } else if (x->type == FGL_STR && y->type == FGL_NONE) {
         return 1;
      } else if (x->type == FGL_STR && y->type == FGL_ULONG) {
         return 1;
      } else if (x->type == FGL_STR && y->type == FGL_STR) {
         // TODO: hostcmp for host names
         return strcmp(x->u.id_str, y->u.id_str);
      }
   }
#endif
   return 0;
}

/**
 * @brief Sort the calling thread's registered requests into locking order
 *
 * Called by #fgl_lock; there is normally no reason to call it directly.
 */
void fgl_rsv_sort() {
   // fetch current lck requests array and pos 
   fgl_t *requests = nullptr;
   uint32_t pos = 0;
   fgl_state_get_requests(&requests, &pos);

   // sort
   qsort(requests, pos, sizeof(fgl_t), fgl_rsv_compare);
}

/**
 * @brief Append one request to the calling thread's request array
 *
 * Aborts the process through `ocs::TerminationManager` when more than
 * #FGL_REQ_MAX requests are registered — the array is fixed size.
 *
 * @param id_root the list the lock belongs to
 * @param is_rw true for a write lock, false for a read lock
 * @param type which kind of element is named
 * @param id_ulong element id when @p type is #FGL_ULONG, otherwise ignored
 * @param id_str element id when @p type is #FGL_STR, otherwise ignored; the
 *        string is not copied and must stay alive until #fgl_unlock
 */
static void fgl_add(uint32_t id_root, bool is_rw, fgl_type_t type, uint32_t id_ulong, const char *id_str) {
   // fetch current lck requests array and pos 
   fgl_t *requests = nullptr;
   uint32_t pos = 0;
   fgl_state_get_requests(&requests, &pos);

   // check fill size of array
   if (pos >= FGL_REQ_MAX) {
      fprintf(stderr, "CRITICAL: table to small. Increase table and recompile.\n");
      ocs::TerminationManager::trigger_abort();
   }

   // set fields of new entry
   requests[pos].id_root = id_root;
   requests[pos].is_rw = is_rw;
   requests[pos].type = type;
   switch (type) {
      case FGL_ULONG:
         requests[pos].u.id_ulong = id_ulong;
         break;
      case FGL_STR:
         requests[pos].u.id_str = id_str;
         break;
      case FGL_NONE:
         break;
   }

   // forward to the next free position
   fgl_state_set_pos(++pos);
}

/**
 * @brief Register a lock on a whole list
 *
 * @param id_root the list to lock
 * @param is_rw true for a write lock, false for a read lock
 */
void fgl_add_r(uint32_t id_root, bool is_rw) {
   fgl_add(id_root, is_rw, FGL_NONE, 0, nullptr);
}

/**
 * @brief Register a lock on one element identified by a number
 *
 * @param id_root the list the element belongs to
 * @param id_ulong the element's id
 * @param is_rw true for a write lock, false for a read lock
 */
void fgl_add_u(uint32_t id_root, uint32_t id_ulong, bool is_rw) {
   fgl_add(id_root, is_rw, FGL_ULONG, id_ulong, nullptr);
}

/**
 * @brief Register a lock on one element identified by a string
 *
 * @param id_root the list the element belongs to
 * @param id_str the element's id; not copied, so it must stay alive until
 *        #fgl_unlock
 * @param is_rw true for a write lock, false for a read lock
 */
void fgl_add_s(uint32_t id_root, const char *id_str, bool is_rw) {
   fgl_add(id_root, is_rw, FGL_STR, 0, id_str);
}

/**
 * @brief Discard the calling thread's registered requests without locking
 *
 * For abandoning a request set that was built but never handed to #fgl_lock.
 * After #fgl_unlock the list is already empty.
 */
void fgl_clear() {
   fgl_state_set_pos(0);
}

/**
 * @brief Render request @p i as the string used to key the lock table
 *
 * @param i index into the calling thread's request array; out of range is ignored
 * @param[out] dstr receives the key
 * @param do_clear true to replace @p dstr with just this key, false to append
 *        the key followed by a newline
 */
static void fgl_get_key_clear(int i, dstring *dstr, bool do_clear) {
   // fetch current array and pos 
   fgl_t *requests = nullptr;
   uint32_t pos = 0;
   fgl_state_get_requests(&requests, &pos);

   // entry does not exist
   if (i >= (int)pos) {
      return;
   }

   // print entry or append it to the string
   if (do_clear) {
      sge_dstring_clear(dstr);
   }
   sge_dstring_sprintf_append(dstr, "%s %u", requests[i].is_rw ? "RW" : "RO", requests[i].id_root);
   switch (requests[i].type) {
      case FGL_ULONG:
         sge_dstring_sprintf_append(dstr, " %u", requests[i].u.id_ulong);
         break;
      case FGL_STR:
         sge_dstring_sprintf_append(dstr, " %s", requests[i].u.id_str);
         break;
      case FGL_NONE:
         break;
   }
   if (!do_clear) {
      sge_dstring_sprintf_append(dstr, "\n");
   }
}

/**
 * @brief Render request @p i as its lock table key, replacing @p dstr
 *
 * @param i index into the calling thread's request array
 * @param[out] dstr receives the key
 */
static void fgl_get_key(int i, dstring *dstr) {
   fgl_get_key_clear(i, dstr, true);
}

/**
 * @brief Append the calling thread's registered requests, one per line
 *
 * Each line is `RW`/`RO`, the root id and the element id, if any.
 *
 * @param[out] dstr the string to append to
 */
void fgl_dump(dstring *dstr) {
   // fetch current array and pos 
   fgl_t *requests = nullptr;
   uint32_t pos = 0;
   fgl_state_get_requests(&requests, &pos);

   // print each entry
   for (uint32_t i = 0; i < pos; i++) {
      fgl_get_key_clear(i, dstr, false);
   }
}

#if COLLECT_STATS

/**
 * @brief Append one lock's statistics; a callback for `sge_htable_for_each_ep`
 *
 * Writes into the module's statistics buffer, which #fgl_dump_stats then hands
 * to the caller, so it must only run under the statistics mutex.
 *
 * @param fgl_stats the statistics table being walked; unused
 * @param key_data the lock key, as a `const char *`
 * @param value_data the entry, as a `fgl_stats_t **`
 */
void fgl_dump_stats_entry(htable fgl_stats, const void *key_data, const void **value_data) {
   const char *key = (const char *)key_data;
   fgl_stats_t **stats = (fgl_stats_t **) value_data;

   sge_dstring_sprintf_append(&fgl_stats_str, "key=%-20s, avg=%.9lf min=%.9lf max=%.9lf measurements=%8u qpos=%u\n",
                              key, (*stats)->avg_wait_time, (*stats)->min_wait_time, (*stats)->max_wait_time,
                              (*stats)->measurements, (*stats)->qpos);
}

#endif

/**
 * @brief Collect the wait time statistics of every lock and reset them
 *
 * Does nothing when #COLLECT_STATS is 0, leaving @p stats_str untouched.
 *
 * @param[out] stats_str receives one line per lock key
 */
void fgl_dump_stats(dstring *stats_str) {
#if COLLECT_STATS
   pthread_mutex_lock(&fgl_stats_mtx);
   sge_htable_for_each_ep(fgl_stats, fgl_dump_stats_entry);
   sge_dstring_sprintf(stats_str, "%s", sge_dstring_get_string(&fgl_stats_str));
   sge_dstring_clear(&fgl_stats_str);
   pthread_mutex_unlock(&fgl_stats_mtx);
#endif
   return;
}

/**
 * @brief Acquire every lock the calling thread registered
 *
 * Sorts the requests first, so all threads acquire a shared set in the same
 * order and cannot deadlock against each other. Locks are created in the
 * process-wide table on first use and reference counted.
 *
 * Each acquired lock must be released with #fgl_unlock.
 *
 * @todo make sure each lock is unique and identify inconsistencies
 */
void fgl_lock() {
   DENTER(TOP_LAYER);

   // fetch request array 
   fgl_t *requests = nullptr;
   uint32_t pos = 0;
   fgl_state_get_requests(&requests, &pos);

   // ensure the lock requests are sorted correctly
   fgl_rsv_sort();
   // TODO: make sure each lock is unique and identify incosistencies

   // handle all requested locks
   for (uint32_t i = 0; i < pos; i++) {
      uint32_t counter;

      // create key for the lock
      dstring key = DSTRING_INIT;
      fgl_get_key(i, &key);

      // lock the locktable
      pthread_mutex_lock(&fgl_mtx);

      // find or create the lock in the table with created key
      fgl_lck_t *fgl_lck = nullptr;
      int found = sge_htable_lookup(fgl_lcks, sge_dstring_get_string(&key), (const void **) &fgl_lck);
      if (found == False) {

         // create data lock and initialize reference rounter 
         fgl_lck = (fgl_lck_t *) sge_malloc(sizeof(fgl_lck_t));
         SGE_ASSERT(fgl_lck != nullptr);
#if USE_FIFO_LOCK
         sge_fifo_lock_init(&fgl_lck->lck);
#else
         pthread_rwlock_init(&fgl_lck->lck, nullptr);
#endif
         counter = 1;
         fgl_lck->counter = counter;

         DPRINTF("lock create \"%s\" %d\n", sge_dstring_get_string(&key), fgl_lck->counter);

         // make the data lock available for others
         sge_htable_store(fgl_lcks, sge_dstring_get_string(&key), fgl_lck);
      } else {
         counter = fgl_lck->counter + 1;
         fgl_lck->counter = counter;
         DPRINTF("lock reuse \"%s\" %d\n", sge_dstring_get_string(&key), fgl_lck->counter);
      }

      // unlock the locktable
      pthread_mutex_unlock(&fgl_mtx);

#if COLLECT_STATS
      struct timeval start_time;
      gettimeofday(&start_time, nullptr);
      suseconds_t start_us = start_time.tv_sec * 10e6 + start_time.tv_usec;
#endif

      DPRINTF("lock \"%s\"\n", sge_dstring_get_string(&key));

      // lock data with RO or RW
#if USE_FIFO_LOCK
      sge_fifo_lock(&fgl_lck->lck, !requests[i].is_rw);
#else
      if (requests[i].is_rw) {
         pthread_rwlock_wrlock(&fgl_lck->lck);
      } else {
         pthread_rwlock_rdlock(&fgl_lck->lck);
      }
#endif

#if COLLECT_STATS
      struct timeval end_time;
      gettimeofday(&end_time, nullptr);
      suseconds_t end_us = end_time.tv_sec * 10e6 + end_time.tv_usec;
      double wait_time = ((double) (end_us - start_us)) / 10e6;

      pthread_mutex_lock(&fgl_stats_mtx);
      fgl_stats_t *stats;
      found = sge_htable_lookup(fgl_stats, sge_dstring_get_string(&key), (const void **) &stats);
      if (found == False) {
         stats = (fgl_stats_t *) sge_malloc(sizeof(fgl_stats_t));
         stats->qpos = counter;
         stats->measurements = 1;
         stats->min_wait_time = wait_time;
         stats->max_wait_time = wait_time;
         stats->avg_wait_time = wait_time;
         sge_htable_store(fgl_stats, sge_dstring_get_string(&key), stats);
      } else {
         stats->qpos = std::max(stats->qpos, counter);
         stats->avg_wait_time = (stats->measurements * stats->avg_wait_time + wait_time) / (stats->measurements + 1);
         stats->measurements++;
         stats->min_wait_time = std::min(stats->min_wait_time, wait_time);
         stats->max_wait_time = std::max(stats->max_wait_time, wait_time);
      }
      pthread_mutex_unlock(&fgl_stats_mtx);
#endif
      sge_dstring_free(&key);
   }
   DRETURN_VOID;
}

/**
 * @brief Release every lock the calling thread holds, and clear the requests
 *
 * Releases in reverse acquisition order and drops each lock's reference count,
 * removing the table entry when it reaches zero. The request array is empty
 * afterwards, so the next round starts fresh without calling #fgl_clear.
 */
void fgl_unlock() {
   DENTER(TOP_LAYER);
   // fetch current array and pos 
   fgl_t *requests = nullptr;
   uint32_t pos = 0;
   fgl_state_get_requests(&requests, &pos);

   // handle all requested locks
   for (int i = pos - 1; i >= 0; i--) {

      // create key for the lock
      dstring key = DSTRING_INIT;
      fgl_get_key(i, &key);

      // lock the locktable
      pthread_mutex_lock(&fgl_mtx);

      // find or create the lock in the table with created key
      fgl_lck_t *fgl_lck = nullptr;
      int found = sge_htable_lookup(fgl_lcks, sge_dstring_get_string(&key), (const void **) &fgl_lck);
      if (found == True) {
         // unlock data 
         DPRINTF("unlock \"%s\"\n", sge_dstring_get_string(&key));
#if USE_FIFO_LOCK
         sge_fifo_ulock(&fgl_lck->lck, !requests[i].is_rw),
#else
         pthread_rwlock_unlock(&fgl_lck->lck);
#endif

         // decrement ref counter
         fgl_lck->counter--;

         // remove lock if ref counter is 0
         if (fgl_lck->counter == 0) {
            DPRINTF("unlock delete \"%s\"\n", sge_dstring_get_string(&key));
            sge_htable_delete(fgl_lcks, sge_dstring_get_string(&key));
            sge_free(&fgl_lck);
         }
      } else {
         // not possible
      }

      // unlock the locktable
      pthread_mutex_unlock(&fgl_mtx);

      // cleanup
      sge_dstring_free(&key);
   }
   DRETURN_VOID;
}

