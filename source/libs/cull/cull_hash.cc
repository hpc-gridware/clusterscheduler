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
};

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
 * @param descr descriptor for the data field in a cull object.
 * @param size initial size of hashtable will be 2^size
 *
 * @return initialized hash description
 */
cull_htable cull_hash_create(const lDescr *descr, int size) {
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

      /* create hashtables, pass final size */
      lDescr *descr = lp->descr;
      for (int i = 0; mt_get_type(descr[i].mt) != lEndT; i++) {
         if (mt_do_hashing(descr[i].mt) && descr[i].ht == nullptr) {
            descr[i].ht = cull_hash_create(&descr[i], size);
         }
      }

      /* create hash entries for all objects */
      for_each_ep_lv(ep, lp) {
         cull_hash_elem(ep);
      }
   }
}

/**
 * @brief Insert a new element in a hash table
 *
 * Stores @p ep under @p key, honouring whether the table holds unique or non
 * unique keys.
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
 * Removes ep from a hash table for data field specified by pos.
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

   ht = ep->descr[pos].ht;


   if (ht == nullptr) {
      return;
   }

   key = cull_hash_key(ep, pos, host_key);
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
 * @brief Insert cull object into associated hash tables
 *
 * Insert the cull element ep into all hash tables that are
 * defined for the cull list ep is member of.
 *
 * @param ep the cull object to be hashed
 */
void cull_hash_elem(const lListElem *ep) {
   int i;
   lDescr *descr;
   char host_key[CL_MAXHOSTNAMELEN];

   if (ep == nullptr) {
      return;
   }

   descr = ep->descr;

   for (i = 0; mt_get_type(descr[i].mt) != lEndT; i++) {
      if (descr[i].ht != nullptr) {
         cull_hash_insert(ep, cull_hash_key(ep, i, host_key), descr[i].ht,
                          mt_is_unique(descr[i].mt));
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
 * @brief Free the hash contents of a cull descr
 *
 * Frees the memory used by the hashing information in a cull
 * descriptor (lDescr). If a hash table is still associated to
 * the descriptor, it is also deleted.
 *
 * @param descr descriptor to free
 *
 * @see `cull_hash_delete_non_unique()`, #sge_htable_destroy
 */
void cull_hash_free_descr(lDescr *descr) {
   int i;
   for (i = 0; mt_get_type(descr[i].mt) != lEndT; i++) {
      cull_htable ht = descr[i].ht;

      if (ht != nullptr) {
         if (!mt_is_unique(descr[i].mt)) {
            /* delete chain of non unique elements */
            sge_htable_for_each_ep(ht->ht, cull_hash_delete_non_unique_chain);
            sge_htable_destroy(ht->nuht);
         }
         sge_htable_destroy(ht->ht);
         sge_free(&(descr[i].ht));
      }
   }
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
      if (descr[pos].ht == nullptr) {
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

   if (descr[pos].ht != nullptr) {
      WARNING(MSG_CULL_HASHTABLEALREADYEXISTS_S, lNm2Str(nm));
      DRETURN(0);
   }

   /* copy hashing information */
   descr[pos].mt |= CULL_HASH;
   if (unique) {
      descr[pos].mt |= CULL_UNIQUE;
   }

   size = hash_compute_size(lGetNumberOfElem(lp));

   descr[pos].ht = cull_hash_create(&descr[pos], size);

   if (descr[pos].ht == nullptr) {
      DRETURN(0);
   }

   /* insert all elements into the new hash table */
   for_each_ep_lv(ep, lp) {
      cull_hash_insert(ep, cull_hash_key(ep, pos, host_key), descr[pos].ht, unique);
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
      lDescr *descr = lp->descr;
      int cleared_hash_index[32];
      int hash_index = 0;

      int size = hash_compute_size(lGetNumberOfElem(lp));

      /* at first free and recreated old non unique hashes */
      for (int i = 0; mt_get_type(descr[i].mt) != lEndT; i++) {
         cull_htable ht = descr[i].ht;
         if (ht != nullptr) {
            if (!mt_is_unique(descr[i].mt)) {
               /* free memory of non unique elements */
               sge_htable_for_each_ep(ht->ht, cull_hash_delete_non_unique_chain);
               sge_htable_destroy(ht->nuht);
               sge_htable_destroy(ht->ht);
               sge_free(&ht);

               /* recreate empty hash */
               descr[i].ht = cull_hash_create(&descr[i], size);

               cleared_hash_index[hash_index] = i;
               hash_index++;
            }
         }
      }

      if (hash_index > 0) {
         char host_key[CL_MAXHOSTNAMELEN];

         /* now insert into the cleared hash list */
         for_each_ep_lv(ep, lp) {
            for (int i = 0; i < hash_index; i++) {
               int index = cleared_hash_index[i];
               cull_hash_insert(ep, cull_hash_key(ep, index, host_key), descr[index].ht, false);
            }
         }
      }
   }
}
