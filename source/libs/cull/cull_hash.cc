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

#include "cull/cull_list.h"
#include "cull/cull_hash.h"
#include "cull/cull_listP.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_multitype.h"
#include "cull/cull_stats.h"
#include "cull/msg_cull.h"

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
   uint32_t dup_count; ///< unique inserts that found the key already taken; see @ref cull_hash_insert
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
static void cull_hash_count_free([[maybe_unused]] const cull_htable ht) {
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
cull_htable cull_hash_create(const lDescr *dp, const int pos, int size) {
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
         unknownType("cull_create_hash");
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
         ret->dup_count = 0;
         ret->pos = pos;
         CULL_STATS_CODE(ret->nm = descr->nm; ret->looked_up = false;)
         CULL_STATS_FIELD(descr->nm, HASH_CREATE, 1);
         CULL_STATS_FIELD(descr->nm, HASH_BUCKET_B, (sizeof(void *) << size) * (nuht != nullptr ? 2 : 1));
      }
   }

   return ret;
}

/**
 * @brief Create all hashtables on a list
 *
 * Creates all hashtables for an empty list.
 *
 * @param lp initialized list structure
 *
 * @note If the list already contains elements, these elements are not
 *       inserted into the hash lists.
 */
void cull_hash_create_hashtables(lList *lp) {
   if (lp != nullptr) {
      /* compute final size of hashtables when all elements are inserted */
      int size = hash_compute_size(lGetNumberOfElem(lp));

      /* create the tables missing in the list's slots, pass final size */
      for (uint32_t i = 0; i < lp->nslots; i++) {
         if (lp->ht[i].ht == nullptr) {
            lp->ht[i].ht = cull_hash_create(lp->descr, lp->ht[i].pos, size);
         }
      }

      /* create hash entries for all objects */
      for_each_ep_lv(ep, lp) {
         cull_hash_elem(ep);
      }
   }
}

/**
 * @brief Name of the field a hash table belongs to
 *
 * @param ep an element of the list the table belongs to
 * @param ht the table to name
 * @return the field's name
 */
static const char *cull_hash_field_name(const lListElem *ep, const cull_htable ht) {
   return lNm2Str(ep->descr[ht->pos].nm);
}

/**
 * @brief The value of the hashed field, for a diagnostic message
 *
 * CS-2782: naming the field alone says where the collision is, not which key
 * collided -- and that is the difference between "somewhere in the event
 * clients" and "twice under id 0". Whoever reads the message is looking for the
 * second thing.
 *
 * @param ep  the element being inserted
 * @param ht  the table it is inserted into, which names the field
 * @param buf buffer for the formatted value
 * @param len size of @p buf
 * @return buf, filled with the value, or "?" for a field type without a key
 */
static const char *cull_hash_key_value(const lListElem *ep, const cull_htable ht, char *buf, size_t len) {
   const int i = ht->pos;
   switch (mt_get_type(ep->descr[i].mt)) {
      case lUlongT:
         snprintf(buf, len, sge_u32, ep->cont[i].ul);
         return buf;
      case lUlong64T:
         snprintf(buf, len, sge_u64, ep->cont[i].ul64);
         return buf;
      case lStringT:
         snprintf(buf, len, SFQ, ep->cont[i].str != nullptr ? ep->cont[i].str : "");
         return buf;
      case lHostT:
         snprintf(buf, len, SFQ, ep->cont[i].host != nullptr ? ep->cont[i].host : "");
         return buf;
      default:
         break;
   }
   snprintf(buf, len, "?");
   return buf;
}

/**
 * @brief Insert a new element in a hash table
 *
 * Stores @p ep under @p key, honouring whether the table holds unique or non
 * unique keys.
 *
 * CS-2755: a unique table can only hold one element per key, so an insert that
 * finds the key taken makes the element already stored there unfindable. The
 * store still happens -- refusing it would only move the damage to the new
 * element, and the callers of lAppendElem() do not check its return value -- but
 * the collision is counted, which is what tells lRemoveElem() that this table
 * needs repairing rather than a plain delete, and what makes the state visible
 * at all.
 *
 * @param ep the cull object to be stored in a hash list
 * @param key the hash key, from #cull_hash_key
 * @param ht the table to insert into
 * @param unique true when the field was declared #CULL_UNIQUE
 */
void cull_hash_insert(const lListElem *ep, void *key, cull_htable ht, bool unique) {
   if (ht == nullptr || ep == nullptr || key == nullptr) {
      return;
   }
   CULL_STATS_FIELD(ht->nm, HASH_INSERT, 1);

   if (unique) {
      const void *stored = nullptr;

      if (sge_htable_lookup(ht->ht, key, &stored) == True && stored != ep) {
         ht->dup_count++;

         /* Only the first one per table is reported. A list that reaches this
          * state usually does so for many keys at once -- a concatenation of two
          * overlapping lists -- and the point of the message is that it happened
          * at all, not how often. */
         if (ht->dup_count == 1) {
            char value[MAX_STRING_SIZE];

            CRITICAL("CULL: two elements share the key " SFN " of field " SFN ", which is "
                     "declared unique; the one stored first is no longer findable "
                     "through it", cull_hash_key_value(ep, ht, value, sizeof(value)),
                     cull_hash_field_name(ep, ht));
         }
      }
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
 * Removes ep from a hash table for data field specified by pos.
 *
 * CS-2755: on a unique table that has seen a duplicate key, the entry is
 * removed only when it is this element's own. The table is keyed by value, so
 * deleting by key alone would take the entry belonging to the other element of
 * the pair and leave it in the list but unfindable. What is left behind after
 * that is put right by #cull_hash_repair, which the callers that know the list
 * run once the element is gone.
 *
 * A table that never saw a collision cannot be holding another element's entry
 * under this key, so it keeps the plain delete and pays nothing for any of this.
 *
 * @param ep the cull object to be removed
 * @param pos position of the data field
 */
void cull_hash_remove(const lListElem *ep, const int pos) {
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

   key = cull_hash_key(ep, pos, host_key);
   if (key != nullptr) {
      if (mt_is_unique(ep->descr[pos].mt)) {
         if (ht->dup_count == 0) {
            /* No key was ever stored twice in this table, so whatever sits
             * under this element's key is this element's own entry. This is the
             * ordinary case and it stays a plain delete. */
            sge_htable_delete(ht->ht, key);
         } else {
            const void *stored = nullptr;

            if (sge_htable_lookup(ht->ht, key, &stored) == True && stored == ep) {
               sge_htable_delete(ht->ht, key);
            }
         }
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
 * @brief Rebuild the unique hash table of one field from the list
 *
 * CS-2755. Only does anything when the table has seen a duplicate key: a
 * removal then cannot simply drop its own entry, because the element left
 * behind may be the one the table has no entry for. Which element that is
 * cannot be worked out from the removed one -- the table is keyed by value, and
 * the list is the only place that knows what is still in it.
 *
 * Rebuilding rather than searching for a single replacement is what
 * #cull_hash_recreate_after_sort already does for the non unique tables, and it
 * keeps the key comparison inside #cull_hash_insert instead of repeating it per
 * type here. The new table counts its own collisions, so the field reports
 * itself clean again as soon as the duplicates are gone.
 *
 * O(n), but only on a list that has actually seen a collision; on every other
 * list this is one comparison.
 *
 * @param lp the list holding the elements, with the removed one already unchained
 * @param pos position of the data field
 */
void cull_hash_rebuild_unique(lList *lp, const int pos) {
   char host_key[CL_MAXHOSTNAMELEN];

   if (lp == nullptr || pos < 0) {
      return;
   }

   cull_hash_slot *slot = list_hash_slot(lp, pos);
   if (slot == nullptr) {
      return;
   }
   cull_htable ht = slot->ht;

   if (ht == nullptr || !mt_is_unique(lp->descr[pos].mt) || ht->dup_count == 0) {
      return;
   }

   cull_htable fresh = cull_hash_create(lp->descr, pos, hash_compute_size(lGetNumberOfElem(lp)));

   /* Without a table the field falls back to the linear search in
    * lGetElemStrFirstRW() and its kind: slower, but still correct. Keeping the
    * old table, which is known to be wrong, would not be. */
   cull_hash_count_free(ht);
   sge_htable_destroy(ht->ht);
   sge_free(&(slot->ht));
   slot->ht = fresh;

   if (fresh == nullptr) {
      return;
   }

   for_each_ep_lv(ep, lp) {
      cull_hash_insert(ep, cull_hash_key(ep, pos, host_key), fresh, true);
   }
}

/**
 * @brief Repair every unique hash table of a list that has seen duplicate keys
 *
 * CS-2755. Call it after removing an element, with the element already
 * unchained so that the rebuild sees the list as it will be. It needs nothing
 * of the removed element, which is why it can run after lFreeElem() has
 * disposed of it.
 *
 * On a list whose keys have always been unique -- every list, until something
 * concatenates two of them -- this is one comparison per hashed field and no
 * work at all.
 *
 * @param lp the list an element was just removed from
 */
void cull_hash_repair(lList *lp) {
   if (lp == nullptr || lp->descr == nullptr) {
      return;
   }

   for (uint32_t i = 0; i < lp->nslots; i++) {
      const cull_htable ht = lp->ht[i].ht;

      /* Tested here rather than left to cull_hash_rebuild_unique() so that the
       * ordinary case costs a load and a branch per hashed field instead of a
       * call. lFreeList() drops the tables before it removes the elements, so
       * this runs once per element of every list being torn down. */
      if (ht != nullptr && ht->dup_count > 0) {
         cull_hash_rebuild_unique(lp, lp->ht[i].pos);
      }
   }
}

/**
 * @brief Insert cull object into associated hash tables
 *
 * Insert the cull element ep into all hash tables of the list it is bound
 * in. An element that is not bound is in no table.
 *
 * @param ep the cull object to be hashed
 */
void cull_hash_elem(const lListElem *ep) {
   char host_key[CL_MAXHOSTNAMELEN];

   if (ep == nullptr) {
      return;
   }

   const lList *lp = elem_list(ep);
   if (lp == nullptr) {
      return;
   }

   for (uint32_t i = 0; i < lp->nslots; i++) {
      const cull_hash_slot &slot = lp->ht[i];
      if (slot.ht != nullptr) {
         cull_hash_insert(ep, cull_hash_key(ep, slot.pos, host_key), slot.ht,
                          mt_is_unique(ep->descr[slot.pos].mt));
      }
   }
}

/**
 * @brief Remove a cull object from all hash tables of its list
 *
 * The counterpart of #cull_hash_elem: removes the bound element ep from every
 * table of the list that holds it. An element that is not bound is in no
 * table, so nothing is done.
 *
 * @param ep the cull object to be removed from the tables
 */
void cull_hash_remove_elem(const lListElem *ep) {
   if (ep == nullptr) {
      return;
   }

   const lList *lp = elem_list(ep);
   if (lp == nullptr) {
      return;
   }

   for (uint32_t i = 0; i < lp->nslots; i++) {
      if (lp->ht[i].ht != nullptr) {
         cull_hash_remove(ep, lp->ht[i].pos);
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
 * necessary data for consecutive calls of cull_hash_next() returning
 * objects with the same hash key.
 *
 * @param ht the table to search
 * @param key the key to use for the search
 * @param unique true when the table holds unique keys
 * @param[out] iterator receives the state #cull_hash_next continues from
 *
 * @return first object found matching key, if no object found: nullptr
 *
 * @see #cull_hash_next
 */
lListElem *cull_hash_first(cull_htable ht, const void *key, bool unique,
                           const void **iterator) {
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
 * to cull_hash_first or cull_hash_next.
 *
 * @param ht the table to search
 * @param[in,out] iterator the state from #cull_hash_first, advanced by this call
 *
 * @return object if found, else nullptr
 *
 * @note The order in which objects with the same key are returned is not
 *       defined.
 *
 * @see #cull_hash_first
 */
lListElem *cull_hash_next(cull_htable ht, const void **iterator) {
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
void cull_hash_delete_non_unique_chain(htable table, const void *key,
                                       const void **data) {
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
static void cull_hash_destroy(cull_htable *htp) {
   cull_htable ht = *htp;
   if (ht == nullptr) {
      return;
   }
   cull_hash_count_free(ht);
   if (ht->nuht != nullptr) {
      /* a non unique table: delete the chain of every key */
      sge_htable_for_each_ep(ht->ht, cull_hash_delete_non_unique_chain);
      sge_htable_destroy(ht->nuht);
   }
   sge_htable_destroy(ht->ht);
   sge_free(htp);
}

/**
 * @brief Give a new list one hash slot per hashed field of its type
 *
 * Called once when a list is created, after its descriptor is set. The slots
 * exist for every field marked #CULL_HASH; whether a table is built in them is
 * the caller's choice, as with lCreateListHash().
 *
 * @param lp the list, with its descriptor and no elements
 * @param create true to build an empty table in every slot
 * @return false if the slot array cannot be allocated
 */
bool cull_hash_init_slots(lList *lp, const bool create) {
   lp->nslots = 0;
   lp->ht = nullptr;

   uint32_t n = 0;
   for (int i = 0; mt_get_type(lp->descr[i].mt) != lEndT; i++) {
      if (mt_do_hashing(lp->descr[i].mt)) {
         n++;
      }
   }
   if (n == 0) {
      return true;
   }

   lp->ht = (cull_hash_slot *) sge_malloc(sizeof(cull_hash_slot) * n);
   if (lp->ht == nullptr) {
      return false;
   }
   for (int i = 0; mt_get_type(lp->descr[i].mt) != lEndT; i++) {
      if (mt_do_hashing(lp->descr[i].mt)) {
         cull_hash_slot &slot = lp->ht[lp->nslots++];
         slot.pos = i;
         slot.ht = create ? cull_hash_create(lp->descr, i, 0) : nullptr;
      }
   }
   return true;
}

/**
 * @brief Free the hash tables of a list and keep its slots
 *
 * The elements stay in the list; they are in no table afterwards, until
 * #cull_hash_create_hashtables builds the tables again.
 *
 * @param lp the list; nullptr is ignored
 */
void cull_hash_free_tables(lList *lp) {
   if (lp == nullptr) {
      return;
   }
   for (uint32_t i = 0; i < lp->nslots; i++) {
      cull_hash_destroy(&lp->ht[i].ht);
   }
}

/**
 * @brief Free the hash tables of a list and its slot array
 *
 * Called when the list itself is freed.
 *
 * @param lp the list; nullptr is ignored
 */
void cull_hash_free_slots(lList *lp) {
   if (lp == nullptr) {
      return;
   }
   cull_hash_free_tables(lp);
   sge_free(&(lp->ht));
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
 * cull_hash_new_check can be used to create a hash table for a list
 * on the contents of a certain field.
 * If it already exist, nothing is done.
 *
 * The caller can choose whether the field contents have to be
 * unique within the list or not.
 *
 * @code
 * create a non unique hash index on the job owner for a job list
 * cull_hash_new_check(job_list, JB_owner, false);
 * @endcode
 *
 * @param lp the list to hold the new hash table
 * @param nm the field on which to create the hash table
 * @param unique unique contents or not
 *
 * @return 1 on success, else 0
 *
 * @see #cull_hash_new
 */
int cull_hash_new_check(lList *lp, int nm, bool unique) {
   const lDescr *descr = lGetListDescr(lp);
   int pos = lGetPosInDescr(descr, nm);

   if (descr != nullptr && pos >= 0) {
      if (list_hash_table(lp, pos) == nullptr) {
         return cull_hash_new(lp, nm, unique);
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
 * cull_hash_new can be used to create a hash table for a list
 * on the contents of a certain field.
 * The caller can choose whether the field contents have to be
 * unique within the list or not.
 *
 * @code
 * create a non unique hash index on the job owner for a job list
 * cull_hash_new(job_list, JB_owner, 0);
 * @endcode
 *
 * @param lp the list to hold the new hash table
 * @param nm the field on which to create the hash table
 * @param unique unique contents or not
 *
 * @return 1 on success, else 0
 */
int cull_hash_new(lList *lp, int nm, bool unique) {
   DENTER(CULL_LAYER);

   lDescr *descr;
   int pos, size;
   char host_key[CL_MAXHOSTNAMELEN];

   if (lp == nullptr) {
      DRETURN(0);
   }

   descr = lp->descr;

   pos = lGetPosInDescr(descr, nm);

   if (pos < 0) {
      CRITICAL(MSG_CULL_XNOTFOUNDINELEMENT_S, lNm2Str(nm));
      DRETURN(0);
   }

   cull_hash_slot *slot = list_hash_slot(lp, pos);
   if (slot != nullptr && slot->ht != nullptr) {
      WARNING(MSG_CULL_HASHTABLEALREADYEXISTS_S, lNm2Str(nm));
      DRETURN(0);
   }

   /* a field that is not hashed in the type gets a slot of its own */
   if (slot == nullptr) {
      auto *slots = (cull_hash_slot *) sge_realloc(lp->ht, sizeof(cull_hash_slot) * (lp->nslots + 1), 0);
      if (slots == nullptr) {
         DRETURN(0);
      }
      lp->ht = slots;
      slot = &lp->ht[lp->nslots++];
      slot->pos = pos;
      slot->ht = nullptr;
   }

   /* copy hashing information */
   descr[pos].mt |= CULL_HASH;
   if (unique) {
      descr[pos].mt |= CULL_UNIQUE;
   }

   size = hash_compute_size(lGetNumberOfElem(lp));

   slot->ht = cull_hash_create(descr, pos, size);

   if (slot->ht == nullptr) {
      DRETURN(0);
   }

   /* insert all elements into the new hash table */
   for_each_ep_lv(ep, lp) {
      cull_hash_insert(ep, cull_hash_key(ep, pos, host_key), slot->ht, unique);
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
void *cull_hash_key(const lListElem *ep, int pos, char *host_key) {
   void *key = nullptr;

   lDescr *descr = &(ep->descr[pos]);

   switch (mt_get_type(descr->mt)) {
      case lUlongT:
         key = (void *) &(ep->cont[pos].ul);
         break;

      case lUlong64T:
         key = (void *) &(ep->cont[pos].ul64);
         break;

      case lStringT:
         key = ep->cont[pos].str;
         break;

      case lHostT:
         if (ep->cont[pos].host != nullptr && host_key != nullptr) {
            sge_hostcpy(host_key, ep->cont[pos].host);
            sge_strtoupper(host_key, CL_MAXHOSTNAMELEN);
            key = host_key;
         }
         break;

      default:
         unknownType("cull_hash_key");
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
cull_hash_statistics(cull_htable ht, dstring *buffer) {
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
 * @brief Rebuild the hash tables of a list after it has been sorted
 *
 * Sorting relinks the elements, so any hash table holding positions has to be
 * built again.
 *
 * @param lp the list whose tables to rebuild; nullptr is ignored
 */
void cull_hash_recreate_after_sort(lList *lp) {
   if (lp != nullptr) {
      bool cleared = false;

      int size = hash_compute_size(lGetNumberOfElem(lp));

      /* at first free and recreate the non unique tables, their chains carry the old order */
      for (uint32_t i = 0; i < lp->nslots; i++) {
         cull_hash_slot &slot = lp->ht[i];
         if (slot.ht != nullptr && !mt_is_unique(lp->descr[slot.pos].mt)) {
            cull_hash_destroy(&slot.ht);
            slot.ht = cull_hash_create(lp->descr, slot.pos, size);
            cleared = true;
         }
      }

      if (cleared) {
         char host_key[CL_MAXHOSTNAMELEN];

         /* now insert into the recreated tables, in the new list order */
         for_each_ep_lv(ep, lp) {
            for (uint32_t i = 0; i < lp->nslots; i++) {
               const cull_hash_slot &slot = lp->ht[i];
               if (slot.ht != nullptr && !mt_is_unique(lp->descr[slot.pos].mt)) {
                  cull_hash_insert(ep, cull_hash_key(ep, slot.pos, host_key), slot.ht, false);
               }
            }
         }
      }
   }
}
