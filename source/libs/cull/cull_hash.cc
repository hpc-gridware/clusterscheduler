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
 ************************************************************************/
/* Portions of this code are Copyright (c) 2011 Univa Corporation. */
/*___INFO__MARK_END__*/

#include <cstdio>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "cull/cull_list.h"
#include "cull/cull_hash.h"
#include "cull/cull_listP.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_multitype.h"
#include "cull/msg_cull.h"

/****** cull/hash/--CULL_Hashtable **********************************************
*  NAME
*     htable -- Hashtable extensions for cull lists 
*
*  SYNOPSIS
*     cull_htable cull_hash_create(const lDescr *descr, int size);
*
*     void cull_hash_new(lList *lp, int name, bool unique);
*
*     void cull_hash_insert(const lListElem *ep, const int pos, );
*
*     void cull_hash_remove(const lListElem *ep, const int pos);
*
*     void cull_hash_elem(const lListElem *ep);
*
*     lListElem *cull_hash_first(const lList *lp, const int pos, 
*                         const void *key, const void **iterator);
*
*     lListElem *cull_hash_next(const lList *lp, const int pos, 
*                         const void *key, const void **iterator);
*
*     void cull_hash_free_descr(lDescr *descr);
*
*  FUNCTION
*     This module provides an abstraction layer between cull and 
*     the hash table implementation in libuti. It provides the 
*     necessary functions to use hash tables from libuti for cull lists.
*
*     The functions defined in this module implement hash tables with 
*     non unique keys, provide wrapper functions for hash insert, remove 
*     and search that are aware of the non unique hash implementation, 
*     functions that deal with the necessary extensions to the cull list
*     descriptor objects etc.
*
*  SEE ALSO
*     uti/hash/--Hashtable
*     cull/hash/cull_hash_create()
*     cull/hash/cull_hash_new()
*     cull/hash/cull_hash_insert()
*     cull/hash/cull_hash_remove()
*     cull/hash/cull_hash_elem()
*******************************************************************************/

/****** cull/hash/-CULL_Hashtable_Defines ****************************************************
*  NAME
*     Defines -- Constants for the cull hash implementation
*
*  SYNOPSIS
*     #define MIN_CULL_HASH_SIZE 4
*
*  FUNCTION
*     Provides constants to be used in the hash table implementation 
*     for cull lists.
*
*     MIN_CULL_HASH_SIZE - minimum size of a hash table. When a new 
*                          hash table is created, it will have the size 
*                          2^MIN_CULL_HASH_SIZE
*******************************************************************************/
#define MIN_CULL_HASH_SIZE 4

/****** cull/hash/-CULL_Hashtable_Typedefs ***************************************************
*  NAME
*     Typedefs -- Typedefs for cull hash implementation 
*
*  SYNOPSIS
*     typedef struct _non_unique_hash non_unique_hash;
*     
*     struct _non_unique_hash {
*        non_unique_hash *next;
*        const void *data;
*     };
*
*  FUNCTION
*     Internal data structure to handle hash tables with non unique 
*     keys. The hash table (from libuti) in this case will not store 
*     a pointer to the cull object itself, but a pointer to a list of 
*     cull objects. This list is implemented using the non_unique_hash 
*     structures.
*
*  SEE ALSO
*     uti/hash/--Hashtable
*******************************************************************************/
typedef struct _non_unique_hash non_unique_hash;

typedef struct non_unique_header {
   non_unique_hash *first;
   non_unique_hash *last;
} non_unique_header;

struct _non_unique_hash {
   non_unique_hash *prev;
   non_unique_hash *next;
   const void *data;
};

struct _cull_htable {
   htable ht;       /* hashtable for keys */
   htable nuht;     /* hashtable for lookup of non unique object references */
};

/****** cull/hash/cull_hash_create() *******************************************
*  NAME
*     cull_hash_create() -- create a new hash table
*
*  SYNOPSIS
*     cull_htable cull_hash_create(const lDescr *descr, int size) 
*
*  FUNCTION
*     Creates a new hash table for a certain descriptor and returns the 
*     hash description (lHash) for it.
*     The initial size of the hashtable can be specified.
*     This allows for optimization of the hashtable, as resize operations
*     can be minimized when the final hashtable size is known at creation time,
*     e.g. when copying complete lists.
*
*  INPUTS
*     const lDescr *descr - descriptor for the data field in a 
*                           cull object.
*     int size            - initial size of hashtable will be 2^size
*
*  RESULT
*     cull_htable - initialized hash description
*******************************************************************************/
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
         ht = sge_htable_create(size, dup_func_u_long32, hash_func_u_long32, hash_compare_u_long32);
         break;
      case lUlong64T:
         ht = sge_htable_create(size, dup_func_u_long64, hash_func_u_long64, hash_compare_u_long64);
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

/****** cull/hash/cull_hash_create_hashtables() ********************************
*  NAME
*     cull_hash_create_hashtables() -- create all hashtables on a list
*
*  SYNOPSIS
*     void cull_hash_create_hashtables(lList *lp) 
*
*  FUNCTION
*     Creates all hashtables for an empty list.
*
*  INPUTS
*     lList *lp - initialized list structure
*
*  NOTES
*     If the list already contains elements, these elements are not 
*     inserted into the hash lists.
*******************************************************************************/
void cull_hash_create_hashtables(lList *lp) {
   if (lp != nullptr) {
      int i, size;
      const lListElem *ep;
      lDescr *descr = lp->descr;

      /* compute final size of hashtables when all elements are inserted */
      size = hash_compute_size(lGetNumberOfElem(lp));

      /* create hashtables, pass final size */
      for (i = 0; mt_get_type(descr[i].mt) != lEndT; i++) {
         if (mt_do_hashing(descr[i].mt) && descr[i].ht == nullptr) {
            descr[i].ht = cull_hash_create(&descr[i], size);
         }
      }

      /* create hash entries for all objects */
      for_each_ep(ep, lp) {
         cull_hash_elem(ep);
      }
   }
}

/****** cull/hash/cull_hash_insert() *******************************************
*  NAME
*     cull_hash_insert() -- insert a new element in a hash table
*
*  SYNOPSIS
*     void cull_hash_insert(const lListElem *ep, const int pos) 
*
*  FUNCTION
*     Inserts ep into the hash list for data field at position pos.
*     A hash key will be computed. The element will be inserted
*     in the corresponding hash table considering unique/non unique
*     hash storage.
*
*  INPUTS
*     const lListElem *ep - the cull object to be stored in a hash list
*     const int pos       - describes the data field of the objects that
*                           is to be hashed
*******************************************************************************/
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

/****** cull/hash/cull_hash_remove() *******************************************
*  NAME
*     cull_hash_remove() -- remove a cull object from a hash list
*
*  SYNOPSIS
*     void cull_hash_remove(const lListElem *ep, const int pos) 
*
*  FUNCTION
*     Removes ep from a hash table for data field specified by pos.
*
*  INPUTS
*     const lListElem *ep - the cull object to be removed
*     const int pos       - position of the data field 
*******************************************************************************/
void cull_hash_remove(const lListElem *ep, const int pos) {
   char host_key[CL_MAXHOSTLEN + 1];
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

/****** cull/hash/cull_hash_elem() *********************************************
*  NAME
*     cull_hash_elem() -- insert cull object into associated hash tables
*
*  SYNOPSIS
*     void cull_hash_elem(const lListElem *ep) 
*
*  FUNCTION
*     Insert the cull element ep into all hash tables that are 
*     defined for the cull list ep is member of.
*
*  INPUTS
*     const lListElem *ep - the cull object to be hashed
*******************************************************************************/
void cull_hash_elem(const lListElem *ep) {
   int i;
   lDescr *descr;
   char host_key[CL_MAXHOSTLEN];

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

/****** cull/hash/cull_hash_first() *******************************************
*  NAME
*     cull_hash_first() -- find first object for a certain key
*
*  SYNOPSIS
*     lListElem* cull_hash_first(const lList *lp, const int pos, 
*                                const void  *key, 
*                                const void **iterator) 
*
*  FUNCTION
*     Searches for key in the hash table for data field described by 
*     pos in the cull list lp.
*     If an element is found, it is returned.
*     If the hash table uses non unique hash keys, iterator returns the 
*     necessary data for consecutive calls of cull_hash_next() returning
*     objects with the same hash key.
*
*  INPUTS
*     const lList *lp       - the cull list to search
*     const int pos         - position of the data field for key
*     const void *key       - the key to use for the search
*     const void **iterator - iterator for calls of cull_hash_next
*
*  RESULT
*     lListElem* - first object found matching key, 
*                  if no object found: nullptr
*
*  SEE ALSO
*     cull/hash/cull_hash_next()
******************************************************************************/
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

/****** cull/hash/cull_hash_next() *********************************************
*  NAME
*     cull_hash_next() -- find next object matching a key
*
*  SYNOPSIS
*     lListElem* cull_hash_next(const lList *lp, const int pos, 
*                               const void *key, const void **iterator) 
*
*  FUNCTION
*     Returns the next object matching the same key as a previous call
*     to cull_hash_first or cull_hash_next.
*
*  INPUTS
*     const lList *lp       - the cull list to search
*     const int pos         - position of the data field for key
*     const void *key       - the key to use for the search
*     const void **iterator - iterator to use for the search.
*
*  RESULT
*     lListElem* - object if found, else nullptr
*
*  NOTES
*     The order in which objects with the same key are returned is not
*     defined.
*
*  SEE ALSO
*     cull/hash/cull_hash_first()
*******************************************************************************/
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

/****** cull/hash/cull_hash_delete_non_unique_chain() *************************
*  NAME
*     cull_hash_delete_non_unique_chain() -- del list of non unique obj.
*
*  SYNOPSIS
*     void cull_hash_delete_non_unique_chain(cull_htable table, 
*                                            const void *key, 
*                                            const void **data) 
*
*  FUNCTION
*     For objects that are stored in a hash table with non unique keys, 
*     for each key a linked list of objects is created.
*     This function deletes this linked list for each key in the hash 
*     table. It is designed to be called by the function 
*     sge_htable_for_each from the libuti hash implementation.
*
*  INPUTS
*     cull_htable table   - hash table in which to delete/free a sublist
*     const void *key   - key of the list to be freed 
*     const void **data - pointer to the sublist
*
*  SEE ALSO
*     uti/hash/sge_htable_for_each_ep()
******************************************************************************/
void cull_hash_delete_non_unique_chain(htable table, const void *key,
                                       const void **data) {
   non_unique_header *head = (non_unique_header *) *data;
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

/****** cull/hash/cull_hash_free_descr() **************************************
*  NAME
*     cull_hash_free_descr() -- free the hash contents of a cull descr
*
*  SYNOPSIS
*     void cull_hash_free_descr(lDescr *descr) 
*
*  FUNCTION
*     Frees the memory used by the hashing information in a cull 
*     descriptor (lDescr). If a hash table is still associated to 
*     the descriptor, it is also deleted.
*
*  INPUTS
*     lDescr *descr - descriptor to free 
*
*  SEE ALSO
*     cull/hash/cull_hash_delete_non_unique()
*     uti/hash/sge_htable_destroy()
******************************************************************************/
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


/****** cull/hash/cull_hash_new_check() ****************************************
*  NAME
*     cull_hash_new() -- create new hash table, if it does not yet exist
*
*  SYNOPSIS
*     int cull_hash_new_check(lList *lp, int nm, bool unique) 
*
*  FUNCTION
*     Usually hash tables are defined in the object type definition
*     for each object type in libs/gdi.
*
*     There are cases where for a certain application additional hash 
*     tables shall be defined to speed up certain access methods.
*
*     cull_hash_new_check can be used to create a hash table for a list
*     on the contents of a certain field.
*     If it already exist, nothing is done.
*
*     The caller can choose whether the field contents have to be
*     unique within the list or not.
*
*  INPUTS
*     lList *lp   - the list to hold the new hash table
*     int nm      - the field on which to create the hash table 
*     bool unique - unique contents or not 
*
*  RESULT
*     int - 1 on success, else 0
*
*  EXAMPLE
*     create a non unique hash index on the job owner for a job list
*     cull_hash_new_check(job_list, JB_owner, false);
*
*  SEE ALSO
*     cull/hash/cull_hash_new()
*
*******************************************************************************/
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

/****** cull/hash/cull_hash_new() **********************************************
*  NAME
*     cull_hash_new() -- create new hash table
*
*  SYNOPSIS
*     int cull_hash_new(lList *lp, int nm, int unique) 
*
*  FUNCTION
*     Usually hash tables are defined in the object type definition
*     for each object type in libs/gdi.
*
*     There are cases where for a certain application additional hash 
*     tables shall be defined to speed up certain access methods.
*
*     cull_hash_new can be used to create a hash table for a list
*     on the contents of a certain field.
*     The caller can choose whether the field contents have to be
*     unique within the list or not.
*
*  INPUTS
*     lList *lp  - the list to hold the new hash table
*     int nm     - the field on which to create the hash table 
*     bool unique - unique contents or not 
*
*  RESULT
*     int - 1 on success, else 0
*
*  EXAMPLE
*     create a non unique hash index on the job owner for a job list
*     cull_hash_new(job_list, JB_owner, 0);
*
*******************************************************************************/
int cull_hash_new(lList *lp, int nm, bool unique) {
   lDescr *descr;
   const lListElem *ep;
   int pos, size;
   char host_key[CL_MAXHOSTLEN];

   DENTER(CULL_LAYER);

   if (lp == nullptr) {
      DRETURN(0);
   }

   descr = lp->descr;

   pos = lGetPosInDescr(descr, nm);

   if (pos < 0) {
      CRITICAL(MSG_CULL_GETELEMSTRERRORXRUNTIMETYPE_S, lNm2Str(nm));
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
   for_each_ep(ep, lp) {
      cull_hash_insert(ep, cull_hash_key(ep, pos, host_key), descr[pos].ht,
                       unique);
   }

   DRETURN(1);
}

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
            sge_strtoupper(host_key, CL_MAXHOSTLEN);
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

void cull_hash_recreate_after_sort(lList *lp) {
   if (lp != nullptr) {
      lDescr *descr = lp->descr;
      int cleared_hash_index[32];
      int i, hash_index = 0;

      int size = hash_compute_size(lGetNumberOfElem(lp));

      /* at first free and recreated old non unique hashes */
      for (i = 0; mt_get_type(descr[i].mt) != lEndT; i++) {
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
         char host_key[CL_MAXHOSTLEN];
         const lListElem *ep;

         /* now insert into the cleared hash list */
         for_each_ep(ep, lp) {
            for (i = 0; i < hash_index; i++) {
               int index = cleared_hash_index[i];
               cull_hash_insert(ep, cull_hash_key(ep, index, host_key), descr[index].ht, false);
            }
         }
      }
   }

   DRETURN_VOID;
}
