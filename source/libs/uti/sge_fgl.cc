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

#include <pthread.h>
#include <cstdio>
#include <cstring>
#include <cctype>

#include "basis_types.h"
#include "uti/sge_dstring.h"
#include "uti/sge_fgl.h"
#include "uti/sge_htable.h"
#include "uti/sge_lock_fifo.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#define COLLECT_STATS 1
#define USE_FIFO_LOCK 0 // mem eater if enabled and slow (one queue and conditions for sync)
#define LOCK_ALL_LISTS_FIRST 0 // 20% slower if enabled

#define FGL_REQ_MAX 128

typedef enum {
   FGL_NONE,
   FGL_ULONG,
   FGL_STR,
} fgl_type_t;

typedef struct {
   u_long32 id_root;
   bool is_rw;
   fgl_type_t type;
   union {
      u_long32 id_ulong;
      const char *id_str;
   } u;
} fgl_t;

typedef struct {
#if USE_FIFO_LOCK
   sge_fifo_rw_lock_t lck;
#else
   pthread_rwlock_t lck;
#endif
   u_long32 counter;
} fgl_lck_t;

typedef struct {
   fgl_t requests[FGL_REQ_MAX];
   u_long32 pos;
} fgl_state_t;

static pthread_once_t fgl_once = PTHREAD_ONCE_INIT;

static pthread_key_t fgl_state_key;

static htable fgl_lcks = nullptr;

static pthread_mutex_t fgl_mtx = PTHREAD_MUTEX_INITIALIZER;

#if COLLECT_STATS
typedef struct {
   u_long32 qpos;
   u_long32 measurements;
   double avg_wait_time;
   double min_wait_time;
   double max_wait_time;
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

static void fgl_state_get_requests(fgl_t **requests, u_long32 *pos) {
   if (requests == nullptr || pos == nullptr) {
      return;
   }

   GET_SPECIFIC(fgl_state_t, fgl_state, fgl_state_init, fgl_state_key);
   *requests = fgl_state->requests;
   *pos = fgl_state->pos;
}

static void fgl_state_set_pos(u_long32 new_pos) {
   GET_SPECIFIC(fgl_state_t, fgl_state, fgl_state_init, fgl_state_key);
   fgl_state->pos = new_pos;
}

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

class FeatureThreadInit {
public:
   FeatureThreadInit() {
      fgl_mt_init();
   }
};

// although not used the constructor call has the side effect to initialize the pthread_key => do not delete
static FeatureThreadInit feature_obj{};


int fgl_rsv_compare(const void *a, const void *b) {
   fgl_t *x = (fgl_t *) a;
   fgl_t *y = (fgl_t *) b;

#if LOCK_ALL_LISTS_FIRST
   // Lists first according to list ID then elements in the same sequence
   if (x->type == FGL_NONE && y->type == FGL_NONE) {
      if (x->id_root < y->id_root) {
         return -1;
      } else if (x->id_root < y->id_root) {
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
      } else if (x->id_root < y->id_root) {
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
      } else if (x->id_root < y->id_root) {
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

void fgl_rsv_sort() {
   // fetch current lck requests array and pos 
   fgl_t *requests = nullptr;
   u_long32 pos = 0;
   fgl_state_get_requests(&requests, &pos);

   // sort
   qsort(requests, pos, sizeof(fgl_t), fgl_rsv_compare);
}

static void fgl_add(u_long32 id_root, bool is_rw, fgl_type_t type, u_long32 id_ulong, const char *id_str) {
   // fetch current lck requests array and pos 
   fgl_t *requests = nullptr;
   u_long32 pos = 0;
   fgl_state_get_requests(&requests, &pos);

   // check fill size of array
   if (pos >= FGL_REQ_MAX) {
      fprintf(stderr, "CRITICAL: table to small. Increase table and recompile.\n");
      abort();
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

void fgl_add_r(u_long32 id_root, bool is_rw) {
   fgl_add(id_root, is_rw, FGL_NONE, 0, nullptr);
}

void fgl_add_u(u_long32 id_root, u_long32 id_ulong, bool is_rw) {
   fgl_add(id_root, is_rw, FGL_ULONG, id_ulong, nullptr);
}

void fgl_add_s(u_long32 id_root, const char *id_str, bool is_rw) {
   fgl_add(id_root, is_rw, FGL_STR, 0, id_str);
}

void fgl_clear() {
   fgl_state_set_pos(0);
}

static void fgl_get_key_clear(int i, dstring *dstr, bool do_clear) {
   // fetch current array and pos 
   fgl_t *requests = nullptr;
   u_long32 pos = 0;
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

static void fgl_get_key(int i, dstring *dstr) {
   fgl_get_key_clear(i, dstr, true);
}

void fgl_dump(dstring *dstr) {
   // fetch current array and pos 
   fgl_t *requests = nullptr;
   u_long32 pos = 0;
   fgl_state_get_requests(&requests, &pos);

   // print each entry
   for (u_long32 i = 0; i < pos; i++) {
      fgl_get_key_clear(i, dstr, false);
   }
}

#if COLLECT_STATS

void fgl_dump_stats_entry(htable fgl_stats, const void *key_data, const void **value_data) {
   const char *key = (const char *)key_data;
   fgl_stats_t **stats = (fgl_stats_t **) value_data;

   sge_dstring_sprintf_append(&fgl_stats_str, "key=%-20s, avg=%.9lf min=%.9lf max=%.9lf measurements=%8u qpos=%u\n",
                              key, (*stats)->avg_wait_time, (*stats)->min_wait_time, (*stats)->max_wait_time,
                              (*stats)->measurements, (*stats)->qpos);
}

#endif

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

void fgl_lock() {
   DENTER(TOP_LAYER);

   // fetch request array 
   fgl_t *requests = nullptr;
   u_long32 pos = 0;
   fgl_state_get_requests(&requests, &pos);

   // ensure the lock requests are sorted correctly
   fgl_rsv_sort();
   // TODO: make sure each lock is unique and identify incosistencies

   // handle all requested locks
   for (u_long32 i = 0; i < pos; i++) {
      u_long32 counter;

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
         stats->qpos = MAX(stats->qpos, counter);
         stats->avg_wait_time = (stats->measurements * stats->avg_wait_time + wait_time) / (stats->measurements + 1);
         stats->measurements++;
         stats->min_wait_time = MIN(stats->min_wait_time, wait_time);
         stats->max_wait_time = MAX(stats->max_wait_time, wait_time);
      }
      pthread_mutex_unlock(&fgl_stats_mtx);
#endif
      sge_dstring_free(&key);
   }
   DRETURN_VOID;
}

void fgl_unlock() {
   DENTER(TOP_LAYER);
   // fetch current array and pos 
   fgl_t *requests = nullptr;
   u_long32 pos = 0;
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

