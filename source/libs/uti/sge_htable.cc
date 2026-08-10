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
 *  Portions of this code are Copyright 2011 Univa Inc.
 * 
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Implementation of the hash table, see @ref uti_htable
 */

/*
 * Based on David Flanagan's Xmt libary's Hash.c
 */

#include <cstdlib>
#include <cstring>

#include "uti/sge_htable.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include <sge_log.h>

#ifdef SGE_USE_PROFILING
#include "uti/sge_profiling.h"
#endif

/** @brief One entry of the hash table
 *
 * Entries whose keys collide are chained through #next.
 */
struct Bucket {
   const void *key;      ///< the table's own copy of the key, made by the dup function
   const void *data;     ///< the value stored under #key, owned by the caller
   struct Bucket *next;  ///< next entry in the same slot, or nullptr
};

/** @brief The hash table itself, hidden behind the #htable handle */
typedef struct _htable_rec {
   Bucket **table;           ///< array of `2^size` slots
   long size;                ///< log2 of the number of slots
   long mask;                ///< number of slots minus 1, used to fold a hash value
   long numentries;          ///< number of entries currently stored
   const void *(*dup_func)(const void *);            ///< copies a key, see @ref uti_htable_dup
   int (*hash_func)(const void *);                   ///< hashes a key, see @ref uti_htable_hash
   int (*compare_func)(const void *, const void *);  ///< compares two keys, see @ref uti_htable_compare
} htable_rec;

/// grow the table when it holds more entries than slots
#define HASH_RESIZE_UP_THRESHOLD 0
/// shrink the table when it holds fewer than half as many entries as slots
#define HASH_RESIZE_DOWN_THRESHOLD 1

/**
 * @brief Resize the hash table
 *
 * Hash tables are dynamically resized if necessary.
 * If the number of elements in a has table becomes too big, the hash
 * algorithm can no longer provide efficient access to the stored
 * objects. On the other hand, storing only a few elements in a much
 * too big hash table wastes memory. Therefore the whole table can
 * be resized. If the hash table has to grow, it is doubled in size.
 * If it has to be shrunk, it is halfed in size.
 *
 * Resizing implies rehashing all stored objects.
 *
 * @param ht the hashtable  to resize
 * @param grow true or false true  = double size of the table, false = shrink table to half the size
 *
 * @note If the system is running in log_level log_debug, statistics is
 *       output before and after resizing the hash table, along with timing
 *       information.
 *
 * @see #sge_htable_statistics
 */

static void sge_htable_resize(htable ht, int grow) {
   DENTER_(BASIS_LAYER);

   Bucket **otable;
   int otablesize;
   Bucket *bucket, *next, **head;
   int i;

#ifdef SGE_USE_PROFILING
   clock_t start = 0;
#endif
   char buffer[1024];
   dstring buffer_wrapper;

   sge_dstring_init(&buffer_wrapper, buffer, sizeof(buffer));

#ifdef SGE_USE_PROFILING
   if(prof_is_active(SGE_PROF_HT_RESIZE) && log_state_get_log_level() >= LOG_DEBUG) {
      struct tms t_buf;
      DEBUG("hash stats before resizing: %s\n", sge_htable_statistics(ht, &buffer_wrapper));
      start = times(&t_buf);
   }
#endif

   otable = ht->table;
   otablesize = 1 << ht->size;

   if (grow) {
      ht->size++;
   } else if (ht->size > 2) {
      ht->size--;
   } else {
      DRETURN_VOID_;
   }

   ht->table = (Bucket **) calloc(1 << ht->size, sizeof(Bucket *));
   ht->mask = (1 << ht->size) - 1;

   for (i = 0; i < otablesize; i++) {
      for (bucket = otable[i]; bucket; bucket = next) {
         next = bucket->next;
         head = &(ht->table[ht->hash_func(bucket->key) & ht->mask]);
         bucket->next = *head;
         *head = bucket;
      }
   }
   sge_free(&otable);

#ifdef SGE_USE_PROFILING
   if(prof_is_active(SGE_PROF_HT_RESIZE) && log_state_get_log_level() >= LOG_DEBUG) {
      struct tms t_buf;
      DEBUG("resizing of hash table took %.3fs\n", (times(&t_buf) - start) * 1.0 / sysconf(_SC_CLK_TCK));
      DEBUG("hash stats after resizing: %s\n", sge_htable_statistics(ht, &buffer_wrapper));
   }
#endif

   DRETURN_VOID_;
}

/**
 * @brief Create a new hash table
 *
 * Creates an empty hash table and initializes its data structures.
 *
 * @code
 * htable ht = sge_htable_create(5, dup_func_uint32_t, hash_func_uint32_t,
 *                               hash_compare_uint32_t);
 * @endcode
 *
 * @param size log2 of the initial number of slots, so the table starts with
 *             `2^size` of them and grows by itself as entries are added
 * @param dup_func copies a key, see @ref uti_htable_dup
 * @param hash_func hashes a key, see @ref uti_htable_hash
 * @param compare_func compares two keys, see @ref uti_htable_compare
 * @return the new hash table, to be released with #sge_htable_destroy
 */
htable sge_htable_create(int size,
                         const void *(*dup_func)(const void *),
                         int (*hash_func)(const void *),
                         int (*compare_func)(const void *, const void *)) {
   auto ht = (htable) sge_malloc(sizeof(htable_rec));

   SGE_ASSERT(ht != nullptr);

   ht->size = size;
   ht->mask = (1 << size) - 1;
   ht->table = (Bucket **) calloc(ht->mask + 1, sizeof(Bucket *));
   ht->numentries = 0;
   ht->dup_func = dup_func;
   ht->hash_func = hash_func;
   ht->compare_func = compare_func;
   return ht;
}

/**
 * @brief Destroy a hash table
 *
 * Destroys a hash table and frees all used memory.
 *
 * @param ht the hash table to destroy
 *
 * @note The objecs managed by the hash table are not destroyed and have
 *       to be handled separately.
 */
void sge_htable_destroy(htable ht) {
   int i;
   Bucket *bucket, *next;

   for (i = 0; i < ht->mask + 1; i++) {
      for (bucket = ht->table[i]; bucket; bucket = next) {
         next = bucket->next;
         if (bucket->key != nullptr) {
            sge_free(&(bucket->key));
         }
         sge_free(&bucket);
      }
   }
   sge_free(&(ht->table));
   sge_free(&ht);
}

/**
 * @brief Apply an action on all elements
 *
 * Calls a certain function for all elements in a hash table.
 *
 * @param table the hash table
 * @param proc func to call for each element
 */
void sge_htable_for_each_ep(htable table, sge_htable_for_each_proc proc) {
   int i;
   Bucket *bucket;

   for (i = 0; i < table->mask + 1; i++) {
      for (bucket = table->table[i]; bucket; bucket = bucket->next)
         (*proc)(table, bucket->key, &bucket->data);
   }
}

/**
 * @brief Store a new element in a hash table
 *
 * Stores a new element in a hash table.
 * If there already exists an element with the same key in the table,
 * it will be replaced by the new element.
 *
 * If the number of elements in the table exceeds the table size, the
 * hash table will be resized.
 *
 * @param table table to hold the new element
 * @param key unique key
 * @param data data to store, usually a pointer to an object
 *
 * @see `sge_htable_resize()`
 */
void sge_htable_store(htable table, const void *key, const void *data) {
   Bucket **head;
   Bucket *bucket;

   head = &(table->table[table->hash_func(key) & table->mask]);
   for (bucket = *head; bucket; bucket = bucket->next) {
      if (table->compare_func(bucket->key, key) == 0) {
         bucket->data = data;
         return;
      }
   }
   bucket = (Bucket *) sge_malloc(sizeof(Bucket));
   SGE_ASSERT(bucket != nullptr);
   bucket->key = table->dup_func(key);
   SGE_ASSERT(bucket->key != nullptr);
   bucket->data = data;
   bucket->next = *head;
   *head = bucket;
   table->numentries++;
   if (table->numentries > (table->mask << HASH_RESIZE_UP_THRESHOLD))
      sge_htable_resize(table, True);
}

/**
 * @brief Search for an element
 *
 * Search for a certain object characterized by a unique key in the
 * hash table.
 * If an element can be found, it is returned in data.
 *
 * @param table the table to search
 * @param key unique key to search
 * @param data object if found, else nullptr
 *
 * @return true, when an object was found, else false
 */
int sge_htable_lookup(htable table, const void *key, const void **data) {
   Bucket *bucket;

   for (bucket = table->table[table->hash_func(key) & table->mask];
        bucket;
        bucket = bucket->next) {
      if (table->compare_func(bucket->key, key) == 0) {
         *data = (void *) bucket->data;
         return True;
      }
   }
   return False;
}

/**
 * @brief Delete an element in a hash table
 *
 * Deletes an element in a hash table.
 * If the number of elements falls below a certain threshold
 * (half the size of the hash table), the hash table is resized
 * (shrunk).
 *
 * @param table hash table that contains the element
 * @param key key of the element to delete
 *
 * @note Only deletes the entry in the hash table. The object itself
 *       is not deleted.
 *
 * @see `sge_htable_resize()`
 */
void sge_htable_delete(htable table, const void *key) {
   Bucket *bucket, **prev;

   for (prev = &(table->table[table->hash_func(key) & table->mask]);
        (bucket = *prev);
        prev = &bucket->next) {
      if (table->compare_func(bucket->key, key) == 0) {
         *prev = bucket->next;
         if (bucket->key != nullptr) {
            sge_free(&(bucket->key));
         }
         sge_free(&bucket);
         table->numentries--;
         if (table->numentries < (table->mask >> HASH_RESIZE_DOWN_THRESHOLD))
            sge_htable_resize(table, False);
         return;
      }
   }
}

/** @brief Number of entries currently stored
 *
 * @param ht the hash table
 * @return the number of entries, not the number of slots
 */
long sge_htable_get_size(htable ht) {
   return ht ? ht->numentries : 0;
}

/**
 * @brief Get some statistics for a hash table
 *
 * Returns a constant string containing statistics for a hash table
 * in the following format:
 * "size: %ld, %ld entries, chains: %ld empty, %ld max, %.1f avg"
 * size is the size of the hash table (number of hash chains)
 * entries is the number of objects stored in the hash table
 * Information about hash chains:
 *    empty is the number of empty hash chains
 *    max is the maximum number of objects in a hash chain
 *    avg is the average number of objects for all occupied
 *    hash chains
 *
 * The string returned is a static buffer, subsequent calls to the
 * function will overwrite this buffer.
 *
 * @param ht Hash table for which statistics shall be generated
 * @param buffer buffer to be provided by caller
 *
 * @return the string described above
 */
const char *sge_htable_statistics(htable ht, dstring *buffer) {
   long empty = 0;
   long max = 0;
   long i;

   /* count empty hash chains and maximum chain length */
   long size = 1 << ht->size;

   for (i = 0; i < size; i++) {
      long count = 0;
      if (ht->table[i] == nullptr) {
         empty++;
      } else {
         Bucket *b = ht->table[i];
         do {
            count++;
         } while ((b = b->next) != nullptr);

         if (count > max) {
            max = count;
         }
      }
   }

   sge_dstring_sprintf_append(buffer,
                              "size: %ld, %ld entries, chains: %ld empty, %ld max, %.1f avg",
                              size, ht->numentries,
                              empty, max,
                              (size - empty) > 0 ? ht->numentries * 1.0 / (size - empty) : 0);

   return sge_dstring_get_string(buffer);
}

/** @brief Duplicate a `uint32_t` key
 *
 * The table keeps its own copy of every key, see @ref uti_htable_dup.
 *
 * @param key the key to copy
 * @return the copy, owned by the hash table
 */
const void *dup_func_uint32_t(const void *key) {
   uint32_t *dup_key = nullptr;
   uint32_t *cast = (uint32_t *) key;

   if ((dup_key = (uint32_t *) sge_malloc(sizeof(uint32_t))) != nullptr) {
      *dup_key = *cast;
   }

   return dup_key;
}

/** @brief Duplicate a `uint64_t` key
 *
 * The table keeps its own copy of every key, see @ref uti_htable_dup.
 *
 * @param key the key to copy
 * @return the copy, owned by the hash table
 */
const void *dup_func_uint64_t(const void *key) {
   uint64_t *dup_key = nullptr;
   uint64_t *cast = (uint64_t *) key;

   if ((dup_key = (uint64_t *) sge_malloc(sizeof(uint64_t))) != nullptr) {
      *dup_key = *cast;
   }

   return dup_key;
}

/** @brief Duplicate a `long` key
 *
 * The table keeps its own copy of every key, see @ref uti_htable_dup.
 *
 * @param key the key to copy
 * @return the copy, owned by the hash table
 */
const void *dup_func_long(const void *key) {
   long *dup_key = nullptr;
   long *cast = (long *) key;

   if ((dup_key = (long *) sge_malloc(sizeof(long))) != nullptr) {
      *dup_key = *cast;
   }
   return dup_key;
}

/** @brief Duplicate a pointer key
 *
 * The table keeps its own copy of every key, see @ref uti_htable_dup.
 *
 * @param key the key to copy
 * @return the copy, owned by the hash table
 */
const void *dup_func_pointer(const void *key) {
   char **dup_key = nullptr;
   char **cast = (char **) key;

   if ((dup_key = (char **) sge_malloc(sizeof(char *))) != nullptr) {
      *dup_key = *cast;
   }
   return dup_key;
}

/** @brief Duplicate a string key
 *
 * The table keeps its own copy of every key, see @ref uti_htable_dup.
 *
 * @param key the key to copy
 * @return the copy, owned by the hash table
 */
const void *dup_func_string(const void *key) {
   return strdup((const char *) key);
}


/** @brief Hash value of a `uint32_t` key
 *
 * See @ref uti_htable_hash.
 *
 * @param key the key to hash
 * @return the hash value
 */
int hash_func_uint32_t(const void *key) {
   uint32_t *cast = (uint32_t *) key;
   return (int) *cast;
}

/** @brief Hash value of a `uint64_t` key
 *
 * See @ref uti_htable_hash.
 *
 * @param key the key to hash
 * @return the hash value
 */
int hash_func_uint64_t(const void *key) {
   uint64_t *cast = (uint64_t *) key;
   return (int) *cast;
}

/** @brief Hash value of a `long` key
 *
 * See @ref uti_htable_hash.
 *
 * @param key the key to hash
 * @return the hash value
 */
int hash_func_long(const void *key) {
   long *cast = (long *) key;
   return (int) *cast;
}

/** @brief Hash value of a pointer key
 *
 * See @ref uti_htable_hash.
 *
 * @param key the key to hash
 * @return the hash value
 */
int hash_func_pointer(const void *key) {
   char **cast = (char **) key;
   long tmp = (long) *cast;
   tmp = tmp >> 7;
/*    printf("====> %p -> %lx -> %x\n", cast, tmp, (int)tmp); */
   return (int) tmp;
}

/** @brief Hash value of a string key
 *
 * See @ref uti_htable_hash.
 *
 * @param key the key to hash
 * @return the hash value
 */
int hash_func_string(const void *key) {
   /* Accumulate in unsigned so overflow is well-defined wraparound instead
    * of signed-overflow UB (UBSan caught the latter on long-running qmasters
    * where hash grew past INT_MAX). Read bytes through unsigned char so
    * high-bit-set bytes zero-extend rather than sign-extend into a negative
    * addend — otherwise non-ASCII strings cluster into a narrow hash range.
    */
   unsigned int hash = 0;
   const auto *c = (const unsigned char *)key;

   if (c != nullptr) {
      do {
         hash += (hash << 3) + *c;
      } while (*c++ != 0);
   }

   return (int) hash;
}

/** @brief Compare two `uint32_t` keys
 *
 * See @ref uti_htable_compare.
 *
 * @param a first key
 * @param b second key
 * @return 0 when equal, > 0 when @p a is greater, < 0 when it is smaller
 */
int hash_compare_uint32_t(const void *a, const void *b) {
   uint32_t *cast_a = (uint32_t *) a;
   uint32_t *cast_b = (uint32_t *) b;
   return *cast_a - *cast_b;
}

/** @brief Compare two `uint64_t` keys
 *
 * See @ref uti_htable_compare.
 *
 * @param a first key
 * @param b second key
 * @return 0 when equal, > 0 when @p a is greater, < 0 when it is smaller
 */
int hash_compare_uint64_t(const void *a, const void *b) {
   uint64_t *cast_a = (uint64_t *) a;
   uint64_t *cast_b = (uint64_t *) b;
   return *cast_a - *cast_b;
}

/** @brief Compare two `long` keys
 *
 * See @ref uti_htable_compare.
 *
 * @param a first key
 * @param b second key
 * @return 0 when equal, > 0 when @p a is greater, < 0 when it is smaller
 */
int hash_compare_long(const void *a, const void *b) {
   long *cast_a = (long *) a;
   long *cast_b = (long *) b;
   return (int) (*cast_a - *cast_b);
}

/** @brief Compare two pointer keys
 *
 * See @ref uti_htable_compare.
 *
 * @param a first key
 * @param b second key
 * @return 0 when equal, > 0 when @p a is greater, < 0 when it is smaller
 */
int hash_compare_pointer(const void *a, const void *b) {
   char **cast_a = (char **) a;
   char **cast_b = (char **) b;
/* printf("++++> %p - %p\n", *cast_a, *cast_b); */
   if (*cast_a != *cast_b) {
      return 1;
   } else {
      return 0;
   }
}

/** @brief Compare two string keys
 *
 * See @ref uti_htable_compare.
 *
 * @param a first key
 * @param b second key
 * @return 0 when equal, > 0 when @p a is greater, < 0 when it is smaller
 */
int hash_compare_string(const void *a, const void *b) {
   return strcmp((const char *) a, (const char *) b);
}

/** @brief Pick an initial table size for an expected number of entries
 *
 * Returns the `size` argument for #sge_htable_create, i.e. log2 of the number
 * of slots, chosen so that a table holding @p number_of_elem entries does not
 * immediately have to grow.
 *
 * @param number_of_elem number of entries the table is expected to hold
 * @return log2 of the number of slots to start with
 */
int hash_compute_size(int number_of_elem) {
   int size = 0;
   while (number_of_elem > 0) {
      size++;
      number_of_elem = number_of_elem >> 1;
   }

   return size;
}

