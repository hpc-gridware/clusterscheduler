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
 * @brief Hash tables over cull list fields
 */

#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
/// Suppresses the monitoring code in the rmon macros for this file
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "tom/ocs_Types.h"
#include "tom/detail/ocs_Hash.h"
#include "tom/ocs_ListCore.h"
#include "tom/detail/ocs_ListP.h"
#include "tom/detail/ocs_MultitypeP.h"
#include "tom/detail/ocs_Stats.h"
#include "tom/msg_tom.h"
#include "tom/ocs_Descr.h"

#include "tom/ocs_ElementCore.h"
#include "tom/ocs_Model.h"

/**
 * @defgroup cull_hash Hash tables for cull lists
 * @brief An abstraction layer between cull and the hash tables in libuti
 *
 * A field marked #CULL_HASH gets a hash table over its values, so looking an
 * element up by that field does not have to walk the list.
 *
 * libuti's hash tables store one value per key. Cull needs **non unique** keys
 * as well, so for those the table stores a pointer to a chain of
 * @ref _non_unique_hash nodes rather than to the element itself. The wrapper
 * functions here hide that difference: insert, remove and search behave the
 * same whether the field was declared #CULL_UNIQUE or not.
 *
 * @see @ref cull_field_attributes
 */


/// A new hash table is created with 2^#MIN_CULL_HASH_SIZE buckets
#define MIN_CULL_HASH_SIZE 4

// the file-local part of the hash layer; the functions are defined below with their qualified names
namespace ocs::tom::detail {
   /// Lists shorter than this are searched linearly by this thread; see ocs::tom::detail::hash_set_min_elems()
   static thread_local uint32_t hash_min_elems = OCS_CULL_HASH_MIN_ELEMS;

   /// Marks a threshold of this thread that was neither set nor read from the environment yet
   constexpr uint32_t HASH_MIN_ELEMS_UNSET = UINT32_MAX;

   /// The threshold of this thread for host fields; see ocs::tom::detail::hash_set_min_elems_host()
   static thread_local uint32_t hash_min_elems_host = HASH_MIN_ELEMS_UNSET;

   /**
    * @brief The threshold of the calling thread for a host field
    *
    * Until the thread sets one, it is taken once from the environment variable `OCS_TOM_HASH_MIN_ELEMS_HOST`,
    * and without it from #OCS_CULL_HASH_MIN_ELEMS_HOST. The variable makes values comparable in a measurement
    * without rebuilding.
    *
    * @return the list length below which a host field is searched linearly
    */
   static uint32_t host_min_elems() {
      if (hash_min_elems_host == HASH_MIN_ELEMS_UNSET) {
         const char *env = getenv("OCS_TOM_HASH_MIN_ELEMS_HOST");
         char *end = nullptr;
         const unsigned long value = env != nullptr && *env != '\0' ? strtoul(env, &end, 10) : 0;
         hash_min_elems_host = env != nullptr && *env != '\0' && *end == '\0' && value < HASH_MIN_ELEMS_UNSET ?
                                  static_cast<uint32_t>(value) : OCS_CULL_HASH_MIN_ELEMS_HOST;
      }
      return hash_min_elems_host;
   }

   /// The threshold of this thread for string fields; see ocs::tom::detail::hash_set_min_elems_string()
   static thread_local uint32_t hash_min_elems_string = HASH_MIN_ELEMS_UNSET;

   /**
    * @brief The threshold of the calling thread for a string field
    *
    * The general threshold of the thread until the thread sets one of its own.
    *
    * @return the list length below which a string field is searched linearly
    */
   static uint32_t string_min_elems() {
      return hash_min_elems_string == HASH_MIN_ELEMS_UNSET ? hash_min_elems : hash_min_elems_string;
   }

   static void hash_destroy(cull_htable *htp);

   static void hash_count_free(cull_htable ht);

   static cull_hash_slot *hash_alloc_slots(const lList *lp);

   static cull_hash_slot *hash_publish_slots(const lList *lp);

   static void hash_delete_non_unique_chain(htable table, const void *key, const void **data);
} // namespace ocs::tom::detail

/// One node of the chain a non unique hash key maps to; see @ref _non_unique_hash
typedef struct _non_unique_hash non_unique_hash;

/// Head and tail of the chain a non unique hash key maps to
typedef struct non_unique_header {
   non_unique_hash *first; ///< first node of the chain
   non_unique_hash *last;  ///< last node, kept so appending stays O(1)
} non_unique_header;

/**
 * @brief One element in the chain a non unique hash key maps to
 *
 * Doubly linked so that removing one element is O(1) once found — the caller
 * already holds the node, and does not have to walk the chain to unlink it.
 */
struct _non_unique_hash {
   non_unique_hash *prev; ///< previous node, or nullptr at the head of the chain
   non_unique_hash *next; ///< next node, or nullptr at the end of the chain
   const void *data;      ///< the cull element this node stands for
};

/**
 * @brief A cull hash table: the key table, plus the chains for non unique keys
 */
struct _cull_htable {
   htable ht;   ///< maps a field value to an element, or to a @ref non_unique_header
   htable nuht; ///< maps an element back to its chain node, so removal need not search
   int pos;            ///< descriptor position of the field the table is built on, for diagnostics
#ifdef OCS_CULL_STATS
   int nm;             ///< the field the table is built on, for the statistics
   bool looked_up;     ///< the table was looked up at least once, for the statistics
#endif
};

/**
 * @brief Count a hash table that is about to be freed (CS-2993)
 * @param ht the table
 */
void ocs::tom::detail::hash_count_free([[maybe_unused]] const cull_htable ht) {
   CULL_STATS_FIELD(ht->nm, HASH_FREE_USED, ht->looked_up ? 1 : 0);
   CULL_STATS_FIELD(ht->nm, HASH_FREE_UNUSED, ht->looked_up ? 0 : 1);
}

/**
 * @brief Create a new hash table
 *
 * Creates a new hash table for a certain descriptor and returns the
 * hash description (lHash) for it.
 * The initial size of the hashtable can be specified.
 * This allows for optimization of the hashtable, as resize operations
 * can be minimized when the final hashtable size is known at creation time,
 * e.g. when copying complete lists.
 *
 * @param dp descriptor of the list the table is for
 * @param pos position of the hashed field in @p dp
 * @param size initial size of hashtable will be 2^size
 *
 * @return initialized hash description
 */
cull_htable ocs::tom::detail::hash_create(const lDescr *dp, const int pos, int size) {
   const lDescr *descr = &dp[pos];
   htable ht = nullptr;   /* hash table for keys */
   htable nuht = nullptr;   /* hash table for non unique access */
   cull_htable ret = nullptr;

   /* if no size is given, use default value */
   if (size == 0) {
      size = MIN_CULL_HASH_SIZE;
   }

   /* create hash table for object keys */
   switch (mt_get_type(descr->mt)) {
      case lStringT:
         ht = sge_htable_create(size, dup_func_string, hash_func_string, hash_compare_string);
         break;
      case lHostT:
         ht = sge_htable_create(size, dup_func_string, hash_func_string, hash_compare_string);
         break;
      case lUlongT:
         ht = sge_htable_create(size, dup_func_uint32_t, hash_func_uint32_t, hash_compare_uint32_t);
         break;
      case lUlong64T:
         ht = sge_htable_create(size, dup_func_uint64_t, hash_func_uint64_t, hash_compare_uint64_t);
         break;
      default:
         ht = nullptr;
         break;
   }

   /* (optionally) create hash table for non unique hash access */
   if (ht != nullptr) {
      if (!mt_is_unique(descr->mt)) {
         nuht = sge_htable_create(size, dup_func_pointer,
                                  hash_func_pointer, hash_compare_pointer);
         if (nuht == nullptr) {
            sge_htable_destroy(ht);
            ht = nullptr;
         }
      }
   }

   /* hashtables OK? Then create cull_htable */
   if (ht != nullptr) {
      ret = (cull_htable) sge_malloc(sizeof(struct _cull_htable));

      /* malloc error? destroy hashtables */
      if (ret == nullptr) {
         sge_htable_destroy(ht);
         if (nuht != nullptr) {
            sge_htable_destroy(nuht);
         }
      } else {
         ret->ht = ht;
         ret->nuht = nuht;
         ret->pos = pos;
         CULL_STATS_CODE(ret->nm = descr->nm; ret->looked_up = false;)
         CULL_STATS_FIELD(descr->nm, HASH_CREATE, 1);
         CULL_STATS_FIELD(descr->nm, HASH_BUCKET_B, (sizeof(void *) << size) * (nuht != nullptr ? 2 : 1));
      }
   }

   return ret;
}

/**
 * @brief Allocate the slot array of a hashable list, with no table in it
 *
 * One slot per field of the list's descriptor that is marked #CULL_HASH, in
 * descriptor order. That includes a field added by ocs::tom::detail::hash_new(), which marks
 * it in the descriptor.
 *
 * @param lp the list, with lp->nslots > 0
 * @return the array of lp->nslots slots, or nullptr if it cannot be allocated
 *         or the descriptor does not match lp->nslots
 */
cull_hash_slot *ocs::tom::detail::hash_alloc_slots(const lList *lp) {
   auto *slots = (cull_hash_slot *) sge_malloc(sizeof(cull_hash_slot) * lp->nslots);
   if (slots == nullptr) {
      return nullptr;
   }
   /* Placement new allocates nothing, it constructs each slot in the memory
    * from sge_malloc() above. A slot holds a std::atomic, which (unlike a plain
    * struct) does not come into existence by malloc alone, so storing into the
    * raw memory would be undefined behaviour. The array is still freed with
    * sge_free(): the slots need no destructor. */
   uint32_t n = 0;
   for (const auto f : ocs::tom::present_fields(lp->descr)) {
      if (mt_do_hashing(lp->descr[f.pos].mt)) {
         if (n == lp->nslots) {
            break;
         }
         new (&slots[n++]) cull_hash_slot{f.pos, nullptr};
      }
   }
   if (n != lp->nslots) {
      // cannot happen while only ocs::tom::detail::hash_new() adds hashed fields; the list stays linear
      sge_free(&slots);
   }
   return slots;
}

/**
 * @brief The slot array of a hashable list, allocated if it has none yet
 *
 * Concurrent first lookups, which run under a read lock, may each allocate an
 * array: the first to publish it by compare-and-swap wins, the others free
 * theirs and use the winner's.
 *
 * @param lp the list, with lp->nslots > 0
 * @return the slot array, or nullptr if it cannot be allocated
 */
cull_hash_slot *ocs::tom::detail::hash_publish_slots(const lList *lp) {
   cull_hash_slot *slots = list_slots(lp);
   if (slots != nullptr) {
      return slots;
   }
   cull_hash_slot *fresh = ocs::tom::detail::hash_alloc_slots(lp);
   if (fresh == nullptr) {
      return nullptr;
   }
   if (lp->ht.compare_exchange_strong(slots, fresh, std::memory_order_acq_rel, std::memory_order_acquire)) {
      return fresh;
   }
   // another reader published its array first; slots now holds that one
   sge_free(&fresh);
   return slots;
}

/**
 * @brief Build the hash table of one field of a list over all its elements
 *
 * The table is private to the caller until it is published in a slot. Building
 * only reads the list, so concurrent readers may each build one. A key of a
 * unique field held by a second element is a logic error (N-H-18): the partial
 * table is freed, nothing is published, and the exception names type, field,
 * key and list; so every later lookup builds the table again and throws again.
 *
 * @param lp the list, not nullptr
 * @param pos position of the field in the list's descriptor
 * @return the table, or nullptr when it cannot be allocated
 * @throw ocs::tom::FieldException with reason DuplicateKey
 */
cull_htable ocs::tom::detail::hash_build(const lList *lp, const int pos) {
   char host_key[CL_MAXHOSTNAMELEN + 1];

   cull_htable ht = ocs::tom::detail::hash_create(lp->descr, pos, hash_compute_size(lp->nelem));
   if (ht != nullptr) {
      const int mt = lp->descr[pos].mt;
      const bool unique = mt_is_unique(mt);
      for (const lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
         void *key = ocs::tom::detail::hash_key(ep, pos, host_key);
         if (unique && key != nullptr) {
            // a unique key held twice: the table is not published, every later lookup builds it again and throws
            if (const void *stored = nullptr; sge_htable_lookup(ht->ht, key, &stored) == True) {
               const std::string text = ocs::tom::key_text(mt, key);
               ocs::tom::detail::hash_destroy(&ht);
               ocs::tom::throw_duplicate_key(lp->descr[pos].nm, lp->descr, text, lp->listname);
            }
         }
         ocs::tom::detail::hash_insert(ep, key, ht, unique);
      }
   }
   return ht;
}

/**
 * @brief The hash table of a field for a lookup, built by the first lookup
 *
 * A list that is hashable and has at least the calling thread's threshold of
 * elements (ocs::tom::detail::hash_set_min_elems()) gets the table of the field on its first lookup, and with
 * the first table of the list its slot array. Concurrent first lookups, which
 * run under a read lock, may each allocate an array and build a table: the
 * first to publish each by compare-and-swap wins, the others free theirs and
 * use the winner's. A table that exists is kept when the list shrinks below
 * the threshold.
 *
 * @param lp the list, not nullptr
 * @param pos position of the field in the list's descriptor
 * @return the table, or nullptr when the list is to be searched linearly
 * @throw ocs::tom::FieldException with reason DuplicateKey when a unique field holds a key twice, see
 *        #ocs::tom::detail::hash_build
 */
cull_htable ocs::tom::detail::hash_lookup_table(const lList *lp, const int pos) {
   // a list that is never hashed, or a field that is not hashed in it, is searched linearly
   if (lp->nslots == 0 || !mt_do_hashing(lp->descr[pos].mt)) {
      return nullptr;
   }

   const uint32_t nelem = lp->nelem;
   CULL_STATS_CODE(
      const int nm = lp->descr[pos].nm;
      // the list length at lookup time, for choosing the threshold
      ocs::tom::stats::add_field(nm, nelem < 4 ? ocs::tom::stats::LOOKUP_LEN_LT4 :
                                     nelem < 8 ? ocs::tom::stats::LOOKUP_LEN_LT8 :
                                     nelem < 16 ? ocs::tom::stats::LOOKUP_LEN_LT16 :
                                     nelem < 32 ? ocs::tom::stats::LOOKUP_LEN_LT32 :
                                     nelem < 64 ? ocs::tom::stats::LOOKUP_LEN_LT64 :
                                     nelem < 128 ? ocs::tom::stats::LOOKUP_LEN_LT128 :
                                     ocs::tom::stats::LOOKUP_LEN_GE128, 1);
   )

   cull_hash_slot *slot = list_hash_slot(lp, pos);
   cull_htable ht = slot != nullptr ? slot->get() : nullptr;
   if (ht != nullptr) {
      return ht;
   }
   const int type = mt_get_type(lp->descr[pos].mt);
   const uint32_t min_elems = type == lHostT ? ocs::tom::detail::host_min_elems()
                            : type == lStringT ? ocs::tom::detail::string_min_elems()
                                               : ocs::tom::detail::hash_min_elems;
   if (nelem < min_elems) {
      CULL_STATS_FIELD(nm, LOOKUP_LINEAR, 1);
      return nullptr;
   }

   if (slot == nullptr) {
      // the first table of the list: its slot array comes with it
      if (ocs::tom::detail::hash_publish_slots(lp) == nullptr || (slot = list_hash_slot(lp, pos)) == nullptr) {
         return nullptr;
      }
      ht = slot->get();
      if (ht != nullptr) {
         return ht;
      }
   }

   cull_htable fresh = ocs::tom::detail::hash_build(lp, pos);
   if (fresh == nullptr) {
      return nullptr;
   }
   if (slot->ht.compare_exchange_strong(ht, fresh, std::memory_order_acq_rel, std::memory_order_acquire)) {
      CULL_STATS_FIELD(nm, HASH_BUILD_LAZY, 1);
      return fresh;
   }
   // another reader published its table first; ht now holds that one
   ocs::tom::detail::hash_destroy(&fresh);
   return ht;
}

/**
 * @brief Set the list length below which the calling thread builds no hash table
 *
 * This is what remains of the per-thread initialisation of the list library
 * since the CULL name space is gone. The value is per thread: a thread that
 * never calls this function uses #OCS_CULL_HASH_MIN_ELEMS. Threads with different values may share lists,
 * since a table, once built, is maintained and used by every thread.
 *
 * The thresholds of host and string fields are set to the same value; ocs::tom::detail::hash_set_min_elems_host()
 * and ocs::tom::detail::hash_set_min_elems_string() set them apart afterwards.
 *
 * @param n the new threshold; 0 builds a table on the first lookup of any hashable list
 */
void ocs::tom::detail::hash_set_min_elems(const uint32_t n) {
   ocs::tom::detail::hash_min_elems = n;
   ocs::tom::detail::hash_min_elems_host = n;
   ocs::tom::detail::hash_min_elems_string = n;
}

/**
 * @brief Set the list length below which the calling thread builds no hash table for a host field
 *
 * Host fields are compared after normalising the stored name, which makes a linear search dearer than for
 * other keys; their threshold can therefore differ. Without a call the environment variable
 * `OCS_TOM_HASH_MIN_ELEMS_HOST` decides, else #OCS_CULL_HASH_MIN_ELEMS_HOST.
 *
 * @param n the new threshold for host fields; 0 builds a table on the first lookup of any hashable list
 */
void ocs::tom::detail::hash_set_min_elems_host(const uint32_t n) {
   ocs::tom::detail::hash_min_elems_host = n == ocs::tom::detail::HASH_MIN_ELEMS_UNSET ? n - 1 : n;
}

/**
 * @brief Set the list length below which the calling thread builds no hash table for a string field
 *
 * A thread that works on lists of its own can hash short string-keyed lists without changing the other threads:
 * the scheduler thread looks up complex entries, queue names and resource names in short lists of its run copy
 * over and over again. Without a call the general threshold of the thread applies.
 *
 * @param n the new threshold for string fields; 0 builds a table on the first lookup of any hashable list
 */
void ocs::tom::detail::hash_set_min_elems_string(const uint32_t n) {
   ocs::tom::detail::hash_min_elems_string = n == ocs::tom::detail::HASH_MIN_ELEMS_UNSET ? n - 1 : n;
}

/**
 * @brief Set the list length below which the calling thread searches a string key field linearly
 *
 * The public form of ocs::tom::detail::hash_set_min_elems_string() for product code outside the list core.
 *
 * @param n the new threshold; 0 builds a hash table on the first lookup of any hashable list
 */
void ocs::tom::set_thread_string_hash_threshold(const std::uint32_t n) {
   ocs::tom::detail::hash_set_min_elems_string(n);
}

/**
 * @brief Set the list length below which the calling thread searches a host key field linearly
 *
 * The public form of ocs::tom::detail::hash_set_min_elems_host() for product code outside the list core.
 *
 * @param n the new threshold; 0 builds a hash table on the first lookup of any hashable list
 */
void ocs::tom::set_thread_host_hash_threshold(const std::uint32_t n) {
   ocs::tom::detail::hash_set_min_elems_host(n);
}

/**
 * @brief Insert a new element in a hash table
 *
 * Stores @p ep under @p key, honouring whether the table holds unique or non
 * unique keys.
 *
 * A unique table never meets a key it holds already for another element: the
 * build refuses a list with a repeated key (#ocs::tom::detail::hash_build) and an
 * insert after a change drops the table instead (#ocs::tom::detail::hash_insert_changed),
 * both since N-H-18 (CS-3052). So a unique insert is a plain store.
 *
 * @param ep the cull object to be stored in a hash list
 * @param key the hash key, from #ocs::tom::detail::hash_key
 * @param ht the table to insert into
 * @param unique true when the field was declared #CULL_UNIQUE
 */
void ocs::tom::detail::hash_insert(const lListElem *ep, void *key, cull_htable ht, bool unique) {
   if (ht == nullptr || ep == nullptr || key == nullptr) {
      return;
   }
   CULL_STATS_FIELD(ht->nm, HASH_INSERT, 1);

   if (unique) {
      sge_htable_store(ht->ht, key, ep);
   } else {
      union {
         non_unique_header *l;
         void *p;
      } head;

      union {
         non_unique_hash *l;
         void *p;
      } nuh;

      head.l = nullptr;
      nuh.l = nullptr;

      /* do we already have a list of elements with this key? */
      if (sge_htable_lookup(ht->ht, key, (const void **) &head.p) == True) {
         /* We only have something to do if ep isn't already stored */
         if (sge_htable_lookup(ht->nuht, &ep, (const void **) &nuh.p) == False) {
            nuh.l = (non_unique_hash *) sge_malloc(sizeof(non_unique_hash));
            nuh.l->data = ep;
            nuh.l->prev = head.l->last;
            nuh.l->next = nullptr;
            nuh.l->prev->next = nuh.l;
            head.l->last = nuh.l;
            sge_htable_store(ht->nuht, &ep, nuh.p);
         }
      } else { /* no list of non unique elements for this key, create new */
         head.l = (non_unique_header *) sge_malloc(sizeof(non_unique_header));
         nuh.l = (non_unique_hash *) sge_malloc(sizeof(non_unique_hash));
         head.l->first = nuh.l;
         head.l->last = nuh.l;
         nuh.l->prev = nullptr;
         nuh.l->next = nullptr;
         nuh.l->data = ep;
         sge_htable_store(ht->ht, key, head.l);
         sge_htable_store(ht->nuht, &ep, nuh.l);
      }
   }
}

/**
 * @brief Remove a cull object from a hash list
 *
 * Removes ep from a hash table for data field specified by pos. A unique
 * table holds each key for one element only (N-H-18), so the entry under the
 * element's key is its own and a plain delete removes it.
 *
 * @param ep the cull object to be removed
 * @param pos position of the data field
 */
void ocs::tom::detail::hash_remove(const lListElem *ep, const int pos) {
   char host_key[CL_MAXHOSTNAMELEN + 1];
   cull_htable ht;
   void *key;

   if (ep == nullptr || pos < 0) {
      return;
   }

   /* only a bound element is hashed, in the tables of the list that holds it */
   ht = elem_hash_table(ep, pos);
   if (ht == nullptr) {
      return;
   }

   key = ocs::tom::detail::hash_key(ep, pos, host_key);
   if (key != nullptr) {
      if (mt_is_unique(ep->descr[pos].mt)) {
         sge_htable_delete(ht->ht, key);
      } else {
         union {
            non_unique_header *l;
            void *p;
         } head;
         union {
            non_unique_hash *l;
            void *p;
         } nuh;

         head.l = nullptr;
         nuh.l = nullptr;

         /* search element in key hashtable */
         if (sge_htable_lookup(ht->ht, key, (const void **) &head.p) == True) {
            /* search element in non unique access hashtable */
            if (sge_htable_lookup(ht->nuht, &ep, (const void **) &nuh.p) == True) {
               if (head.l->first == nuh.p) {
                  head.l->first = nuh.l->next;
                  if (head.l->last == nuh.p) {
                     head.l->last = nullptr;
                  } else {
                     head.l->first->prev = nullptr;
                  }
               } else if (head.l->last == nuh.p) {
                  head.l->last = nuh.l->prev;
                  head.l->last->next = nullptr;
               } else {
                  nuh.l->prev->next = nuh.l->next;
                  nuh.l->next->prev = nuh.l->prev;
               }

               sge_htable_delete(ht->nuht, &ep);
               sge_free(&(nuh.p));
            } else {
               /* JG: TODO: error output */
            }

            if (head.l->first == nullptr && head.l->last == nullptr) {
               sge_free(&head.p);
               sge_htable_delete(ht->ht, key);
            }
         }
      }
   }
}

/**
 * @brief Insert an element into the table of one field after the element was added or its value changed
 *
 * The insertion of the modifying paths: appending, inserting and setting.
 * When the field is unique and its table already holds the key for another
 * element, the table is not trusted any more: it is freed and unpublished,
 * so that the next lookup builds it again from the list (N-H-18). The
 * modification itself goes on; a debug build has stopped it before with its
 * own check. Runs under exclusive access to the list, like every
 * modification.
 *
 * @param ep the element, bound to a list
 * @param pos position of the field
 */
void ocs::tom::detail::hash_insert_changed(const lListElem *ep, const int pos) {
   const cull_htable ht = elem_hash_table(ep, pos);
   if (ht == nullptr) {
      return;
   }
   char host_key[CL_MAXHOSTNAMELEN + 1];
   void *key = ocs::tom::detail::hash_key(ep, pos, host_key);
   if (key == nullptr) {
      return;
   }
   const bool unique = mt_is_unique(ep->descr[pos].mt);
   if (unique) {
      const void *stored = nullptr;
      if (sge_htable_lookup(ht->ht, key, &stored) == True && stored != ep) {
         cull_hash_slot *slot = list_hash_slot(elem_list(ep), pos);
         cull_htable dropped = slot->get();
         slot->set(nullptr);
         ocs::tom::detail::hash_destroy(&dropped);
         return;
      }
   }
   ocs::tom::detail::hash_insert(ep, key, ht, unique);
}

/**
 * @brief Insert cull object into associated hash tables
 *
 * Insert the cull element ep into all hash tables of the list it is bound
 * in. An element that is not bound is in no table. A unique table that
 * already holds one of the element's keys is dropped instead
 * (#ocs::tom::detail::hash_insert_changed).
 *
 * @param ep the cull object to be hashed
 */
void ocs::tom::detail::hash_elem(const lListElem *ep) {
   if (ep == nullptr) {
      return;
   }

   const lList *lp = elem_list(ep);
   if (lp == nullptr) {
      return;
   }

   const cull_hash_slot *slots = list_slots(lp);
   if (slots == nullptr) {
      return;
   }
   for (uint32_t i = 0; i < lp->nslots; i++) {
      ocs::tom::detail::hash_insert_changed(ep, slots[i].pos);
   }
}

/**
 * @brief Remove a cull object from all hash tables of its list
 *
 * The counterpart of #ocs::tom::detail::hash_elem: removes the bound element ep from every
 * table of the list that holds it. An element that is not bound is in no
 * table, so nothing is done.
 *
 * @param ep the cull object to be removed from the tables
 */
void ocs::tom::detail::hash_remove_elem(const lListElem *ep) {
   if (ep == nullptr) {
      return;
   }

   const lList *lp = elem_list(ep);
   if (lp == nullptr) {
      return;
   }

   const cull_hash_slot *slots = list_slots(lp);
   if (slots == nullptr) {
      return;
   }
   for (uint32_t i = 0; i < lp->nslots; i++) {
      if (slots[i].get() != nullptr) {
         ocs::tom::detail::hash_remove(ep, slots[i].pos);
      }
   }
}

/**
 * @brief Find first object for a certain key
 *
 * Searches for key in the hash table for data field described by
 * pos in the cull list lp.
 * If an element is found, it is returned.
 * If the hash table uses non unique hash keys, iterator returns the
 * necessary data for consecutive calls of ocs::tom::detail::hash_next() returning
 * objects with the same hash key.
 *
 * @param ht the table to search
 * @param key the key to use for the search
 * @param unique true when the table holds unique keys
 * @param[out] iterator receives the state #ocs::tom::detail::hash_next continues from
 *
 * @return first object found matching key, if no object found: nullptr
 *
 * @see #ocs::tom::detail::hash_next
 */
lListElem *ocs::tom::detail::hash_first(cull_htable ht, const void *key, bool unique, const void **iterator) {
   union {
      lListElem *l;
      void *p;
   } ep;
   ep.l = nullptr;

   if (iterator == nullptr) {
      return nullptr;
   }

   if (ht == nullptr || key == nullptr) {
      *iterator = nullptr;
      return nullptr;
   }
   CULL_STATS_CODE(ht->looked_up = true;)
   CULL_STATS_FIELD(ht->nm, HASH_LOOKUP, 1);

   if (unique) {
      *iterator = nullptr;
      if (sge_htable_lookup(ht->ht, key, (const void **) &ep.p) == True) {
         return (lListElem *) ep.p;
      } else {
         return nullptr;
      }
   } else {
      union {
         non_unique_header *l;
         void *p;
      } head;
      head.l = nullptr;

      if (sge_htable_lookup(ht->ht, key, (const void **) &head.p) == True) {
         ep.p = (lListElem *) head.l->first->data;
         *iterator = head.l->first;
         return (lListElem *) ep.p;
      } else {
         *iterator = nullptr;
         return nullptr;
      }
   }
}

/**
 * @brief Find next object matching a key
 *
 * Returns the next object matching the same key as a previous call
 * to ocs::tom::detail::hash_first or ocs::tom::detail::hash_next.
 *
 * @param ht the table to search
 * @param[in,out] iterator the state from #ocs::tom::detail::hash_first, advanced by this call
 *
 * @return object if found, else nullptr
 *
 * @note The order in which objects with the same key are returned is not
 *       defined.
 *
 * @see #ocs::tom::detail::hash_first
 */
lListElem *ocs::tom::detail::hash_next(cull_htable ht, const void **iterator) {
   lListElem *ep = nullptr;
   non_unique_hash *nuh = (non_unique_hash *) *iterator;

   if (ht == nullptr) {
      return nullptr;
   }

   nuh = nuh->next;
   if (nuh != nullptr) {
      ep = (lListElem *) nuh->data;
      *iterator = nuh;
   } else {
      *iterator = nullptr;
   }

   return ep;
}

/**
 * @brief Del list of non unique obj
 *
 * For objects that are stored in a hash table with non unique keys,
 * for each key a linked list of objects is created.
 * This function deletes this linked list for each key in the hash
 * table. It is designed to be called by the function
 * sge_htable_for_each from the libuti hash implementation.
 *
 * @param table hash table in which to delete/free a sublist
 * @param key key of the list to be freed
 * @param data pointer to the sublist
 *
 * @see #sge_htable_for_each_ep
 */
void ocs::tom::detail::hash_delete_non_unique_chain(htable table, const void *key, const void **data) {
   auto *head = (non_unique_header *) *data;
   if (head != nullptr) {
      non_unique_hash *nuh = head->first;
      while (nuh != nullptr) {
         non_unique_hash *del = nuh;
         nuh = nuh->next;
         sge_free(&del);
      }
      sge_free(&head);
   }
}

/**
 * @brief Free one hash table, the chains of a non unique table included
 *
 * @param htp the table to free; set to nullptr. A nullptr table is ignored.
 */
void ocs::tom::detail::hash_destroy(cull_htable *htp) {
   cull_htable ht = *htp;
   if (ht == nullptr) {
      return;
   }
   ocs::tom::detail::hash_count_free(ht);
   if (ht->nuht != nullptr) {
      /* a non unique table: delete the chain of every key */
      sge_htable_for_each_ep(ht->ht, ocs::tom::detail::hash_delete_non_unique_chain);
      sge_htable_destroy(ht->nuht);
   }
   sge_htable_destroy(ht->ht);
   sge_free(htp);
}

/**
 * @brief Mark a new list as hashable, with one slot per hashed field of its type
 *
 * Called when a list is created, after its descriptor is set, and by
 * #ocs::tom::make_hashable on a list that becomes hashable. Only the number
 * of hashed fields is recorded; a list with none is never hashed. The slot
 * array is allocated with the first table of the list, by the first lookup on
 * a field of a long enough list (#ocs::tom::detail::hash_lookup_table), so a list that
 * is never searched costs no allocation.
 *
 * @param lp the list, with its descriptor and no slot array
 * @param hashable false to leave the list unhashable, so that it is always searched linearly
 * @return true; kept for the callers, which treated a failed slot allocation here
 */
bool ocs::tom::detail::hash_init_slots(lList *lp, const bool hashable) {
   lp->nslots = 0;
   // the list comes from sge_malloc()/calloc(): construct the atomic before it is used
   new (&lp->ht) std::atomic<cull_hash_slot *>(nullptr);
   if (!hashable) {
      return true;
   }

   for (const auto f : ocs::tom::present_fields(lp->descr)) {
      if (mt_do_hashing(lp->descr[f.pos].mt)) {
         lp->nslots++;
      }
   }
   return true;
}

/**
 * @brief Free the hash tables of a list and keep its slots
 *
 * The elements stay in the list; they are in no table afterwards, until
 * the next lookup on a field builds its table again.
 *
 * @param lp the list; nullptr is ignored
 */
void ocs::tom::detail::hash_free_tables(lList *lp) {
   if (lp == nullptr) {
      return;
   }
   // a list that was never searched has no slot array, and so no table
   cull_hash_slot *slots = list_slots(lp);
   if (slots == nullptr) {
      return;
   }
   for (uint32_t i = 0; i < lp->nslots; i++) {
      cull_htable ht = slots[i].get();
      ocs::tom::detail::hash_destroy(&ht);
      slots[i].set(nullptr);
   }
}

/**
 * @brief Free the hash tables of a list and its slot array
 *
 * Called when the list itself is freed.
 *
 * @param lp the list; nullptr is ignored
 */
void ocs::tom::detail::hash_free_slots(lList *lp) {
   if (lp == nullptr) {
      return;
   }
   ocs::tom::detail::hash_free_tables(lp);
   cull_hash_slot *slots = list_slots(lp);
   sge_free(&slots);
   lp->ht.store(nullptr, std::memory_order_release);
   lp->nslots = 0;
}


/**
 * @brief Create new hash table, if it does not yet exist
 *
 * Usually hash tables are defined in the object type definition
 * for each object type in libs/gdi.
 *
 * There are cases where for a certain application additional hash
 * tables shall be defined to speed up certain access methods.
 *
 * ocs::tom::detail::hash_new_check can be used to create a hash table for a list
 * on the contents of a certain field.
 * If it already exist, nothing is done.
 *
 * The caller can choose whether the field contents have to be
 * unique within the list or not.
 *
 * @code
 * create a non unique hash index on the job owner for a job list
 * ocs::tom::detail::hash_new_check(job_list, JB_owner, false);
 * @endcode
 *
 * @param lp the list to hold the new hash table
 * @param nm the field on which to create the hash table
 * @param unique unique contents or not
 *
 * @return 1 on success, else 0
 *
 * @see #ocs::tom::detail::hash_new
 */
int ocs::tom::detail::hash_new_check(lList *lp, int nm, bool unique) {
   const lDescr *descr = lp != nullptr ? lp->descr : nullptr;
   int pos = descr != nullptr ? ocs::tom::present_pos(descr, nm) : -1;

   if (descr != nullptr && pos >= 0) {
      if (list_hash_slot(lp, pos) == nullptr) {
         return ocs::tom::detail::hash_new(lp, nm, unique);
      }
   }

   return 1;
}

/**
 * @brief Create new hash table
 *
 * Usually hash tables are defined in the object type definition
 * for each object type in libs/gdi.
 *
 * There are cases where for a certain application additional hash
 * tables shall be defined to speed up certain access methods.
 *
 * ocs::tom::detail::hash_new can be used to create a hash table for a list
 * on the contents of a certain field. Like every other table, it is built by
 * the first lookup on that field.
 * The caller can choose whether the field contents have to be
 * unique within the list or not.
 *
 * @code
 * create a non unique hash index on the job owner for a job list
 * ocs::tom::detail::hash_new(job_list, JB_owner, 0);
 * @endcode
 *
 * @param lp the list to hold the new hash table
 * @param nm the field on which to create the hash table
 * @param unique unique contents or not
 *
 * @return 1 on success, else 0
 */
int ocs::tom::detail::hash_new(lList *lp, int nm, bool unique) {
   DENTER(CULL_LAYER);

   const lDescr *descr;
   int pos;

   if (lp == nullptr) {
      DRETURN(0);
   }

   descr = lp->descr;

   pos = ocs::tom::present_pos(descr, nm);

   if (pos < 0) {
      CRITICAL(MSG_CULL_XNOTFOUNDINELEMENT_S, ocs::tom::field_name(nm));
      DRETURN(0);
   }

   cull_hash_slot *slot = list_hash_slot(lp, pos);
   if (slot != nullptr && slot->get() != nullptr) {
      WARNING(MSG_TOM_HASHTABLEALREADYEXISTS_S, ocs::tom::field_name(nm));
      DRETURN(0);
   }

   // a list that is never hashed becomes hashable, with the hashed fields of its type
   if (lp->nslots == 0) {
      ocs::tom::detail::hash_init_slots(lp, true);
   }

   /* A field that is not hashed in the type gets a slot of its own, counted in
    * nslots and marked in the descriptor below, so that the slot array the
    * first lookup allocates from the descriptor contains it. A list that has
    * an array already gets the slot added to it. The slots hold atomics:
    * realloc() would copy their bytes without constructing them, so the slots
    * are constructed in a new array by placement new instead (see
    * ocs::tom::detail::hash_alloc_slots()). */
   if (!mt_do_hashing(descr[pos].mt)) {
      cull_hash_slot *old = list_slots(lp);
      if (old != nullptr) {
         auto *slots = (cull_hash_slot *) sge_malloc(sizeof(cull_hash_slot) * (lp->nslots + 1));
         if (slots == nullptr) {
            DRETURN(0);
         }
         for (uint32_t i = 0; i < lp->nslots; i++) {
            new (&slots[i]) cull_hash_slot{old[i].pos, old[i].get()};
         }
         new (&slots[lp->nslots]) cull_hash_slot{pos, nullptr};
         sge_free(&old);
         lp->ht.store(slots, std::memory_order_release);
      }
      lp->nslots++;
   }

   /* The descriptor is shared with other lists, or is the static block of the
    * type, so the hashing information goes into a private copy that only this
    * list and its elements hold (copy on write). */
   const int mt = descr[pos].mt | CULL_HASH | (unique ? CULL_UNIQUE : 0);
   if (mt != descr[pos].mt) {
      const lDescr *copy = ocs::tom::descr_private(descr);
      if (copy == nullptr) {
         DRETURN(0);
      }
      // the copy is not registered and nobody holds it yet, so it may still be written; the flags are added, so
      // that the reference count mark the copy carries in its first entry stays
      const_cast<lDescr *>(copy)[pos].mt |= CULL_HASH | (unique ? CULL_UNIQUE : 0);
      for (lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
         ep->descr = copy;
      }
      ocs::tom::release_descr(lp->descr);
      lp->descr = copy;
   }

   DRETURN(1);
}

/**
 * @brief The hash key for one field of an element
 *
 * @param ep the element
 * @param pos position of the field in the descriptor
 * @param host_key scratch buffer used when the field is a host name, which is
 *        normalised before hashing
 * @return the key, pointing either into @p ep or into @p host_key
 */
void *ocs::tom::detail::hash_key(const lListElem *ep, int pos, char *host_key) {
   void *key = nullptr;

   const lDescr *descr = &(ep->descr[pos]);
   const lMultiType &value = ocs::tom::detail::value(ep, pos);

   switch (mt_get_type(descr->mt)) {
      case lUlongT:
         key = (void *) &(value.ul);
         break;

      case lUlong64T:
         key = (void *) &(value.ul64);
         break;

      case lStringT:
         key = value.str;
         break;

      case lHostT:
         if (value.host != nullptr && host_key != nullptr) {
            sge_hostcpy(host_key, value.host);
            sge_strtoupper(host_key, CL_MAXHOSTNAMELEN);
            key = host_key;
         }
         break;

      default:
         key = nullptr;
         break;
   }

   return key;
}


/**
 * @brief Render fill level and collision statistics of a hash table
 *
 * @param ht the table to report on
 * @param[out] buffer receives the statistics
 * @return the statistics text, pointing into @p buffer
 */
const char *
ocs::tom::detail::hash_statistics(cull_htable ht, dstring *buffer) {
   const char *ret = nullptr;

   sge_dstring_clear(buffer);

   if (ht != nullptr) {
      sge_dstring_copy_string(buffer, "Keys:\n");
      ret = sge_htable_statistics(ht->ht, buffer);

      if (ht->nuht != nullptr) {
         sge_dstring_append(buffer, "\nNon Unique Hash Access:\n");
         ret = sge_htable_statistics(ht->nuht, buffer);
      }
   } else {
      ret = sge_dstring_copy_string(buffer, "no hash table");
   }

   return ret;
}

/**
 * @brief Drop the non unique hash tables of a list after it has been sorted
 *
 * The chain of a non unique key holds its elements in list order, which the
 * sort has changed, so these tables are freed; the next lookup on such a field
 * builds the table again in the new order. Unique tables map a key to one
 * element and carry no order, so they stay.
 *
 * @param lp the sorted list; nullptr is ignored
 */
void ocs::tom::detail::hash_recreate_after_sort(lList *lp) {
   cull_hash_slot *slots = lp != nullptr ? list_slots(lp) : nullptr;
   if (slots != nullptr) {
      for (uint32_t i = 0; i < lp->nslots; i++) {
         cull_hash_slot &slot = slots[i];
         cull_htable ht = slot.get();
         if (ht != nullptr && !mt_is_unique(lp->descr[slot.pos].mt)) {
            ocs::tom::detail::hash_destroy(&ht);
            slot.set(nullptr);
         }
      }
   }
}
