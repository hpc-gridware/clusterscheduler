#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_htable.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull_listP.h"
#include "cull/cull_multitype.h"
#include "cull/cull_state.h"

#ifdef OBSERVE
#  include "cull/cull_observe.h"
#endif

#ifdef OBSERVE

#define OBSERVE_LAYER BASIS_LAYER

// Defines for RO and RW access
enum {
   OBSERVE_READ = 0,
   OBSERVE_WRITE = 1,
};

// Show all attribute changes and not just those that trigger a switch from RO to RW
static bool show_all = true;

// Internal htable that stores all cull list and element pointers
static htable ob_ht = nullptr;

// Mutex to secure ob_ht
static pthread_mutex_t ob_mtx = PTHREAD_MUTEX_INITIALIZER;

typedef struct _lObserveEntry {
   const void *pointer;     // pointer to a CULL list or element
   const void *owner;       // owner of pointer when pointer is bound in CULL, otherwise nullptr
   bool is_list;            // true if pointer is a CULL list
   int direct_access;       // 1 (RW) when either an attribute was changed directlry or if change in elemts caused change of htable of a list 
   int indirect_access;     // 1 (RW) when some descendant was changed
   bool is_master_list;     // true if pointer is a master list
   const char *list_name;   // master list name if pointer is a master list
} lObserveEntry;

static lObserveEntry *lObserveMallocEntry(void) {
   return (lObserveEntry *)sge_malloc(sizeof(lObserveEntry));
}

#if 0
static void lObserveFreeEntry(htable ht, const void *pointer, const void **entry) {
   sge_free(entry);
}
#endif

#if 0
void lObserveDumpEntry(htable ht, const void *key, const void **e) {
   lObserveEntry *entry = (lObserveEntry *)*e;
   fprintf(stderr, "pointer=%p owner=%p (%s) is_list=%d is_master_list=%d direct=%d indirect=%d\n", 
           entry->pointer, entry->owner, lNm2Str(entry->nm), 
           entry->is_list, entry->is_master_list, entry->direct_access, entry->indirect_access);
}

void lObserveDumpAccess(void) {
   dstring dstr = DSTRING_INIT;

   pthread_mutex_lock(&ob_mtx);
   sge_htable_statistics(ob_ht, &dstr);
   pthread_mutex_unlock(&ob_mtx);
}

void _lObserveDumpAccess(void) {
   sge_htable_for_each_ep(ob_ht, lObserveDumpEntry);
}
#endif

/*
 * @brief lObserveClearEntry resets the internal flags 
 *
 * ... for one pointer that store information about RO or RW access
 */
static void lObserveClearEntry(htable ht, const void *key, const void **e) {
   lObserveEntry *entry = (lObserveEntry *)*e;

   entry->direct_access = OBSERVE_READ;
   entry->indirect_access = OBSERVE_READ;
}

/*
 * @brief lObserveClear clear intenal flags for all observed pointers
 */
static void lObserveClear(void) {
   pthread_mutex_lock(&ob_mtx);
   sge_htable_for_each_ep(ob_ht, lObserveClearEntry);
   pthread_mutex_unlock(&ob_mtx);

   dstring *ob_dstring = cull_state_get_observe_dstring();
   sge_dstring_clear(ob_dstring);
}

/*
 * @brief Returns recordings of pointer observation.
 *
 * The contained string has one entry per
 *    - master list that is access RO
 *    - per RW access (direct or indirect) 
 *
 * lObserveStart() clears the internal dstring and starts observation
 * This function should be called before lObserveEnd() is called. 
 */
void lObserveGetInfoString(dstring *dstr) {
   dstring *info = cull_state_get_observe_dstring();

   sge_dstring_sprintf(dstr, "%s", sge_dstring_get_string(info));
}

/*
 * @brief Returns the internal size of the hashtable.
 */
long lObserveGetSize(void) {
   pthread_mutex_lock(&ob_mtx);
   long l = sge_htable_get_size(ob_ht);
   pthread_mutex_unlock(&ob_mtx);
   return l;
}

/*
 * @brief Initializes this module.
 *
 * This function will be called as part of lInit().
 */
void lObserveInit(void) {
   pthread_mutex_lock(&ob_mtx);
   if (ob_ht == nullptr) {
      ob_ht = sge_htable_create(2^20, dup_func_u_long64, hash_func_u_long64, hash_compare_u_long64);
   }
   pthread_mutex_unlock(&ob_mtx);
}

/*
 * @brief Start observation. Clears 'old' information in advance.
 */
void lObserveStart(void) {
   bool started = cull_state_get_observe_started();
   if (!started) {
      lObserveClear();
      cull_state_set_observe_started(true);
   }
}

/*
 * @brief Stops observation. 
 */
void lObserveEnd(void) {
   bool started = cull_state_get_observe_started();

   if (started) {
      cull_state_set_observe_started(false);
   }
}

/*
 * @brief Adds a new pointer to be observed.
 *
 * @param[in] pointer Pointer to a CULL element or list
 * @param[in] owner   Pointer to the owner of pointer (The binding CULL element or list)
 * @param[in] is_list true if pointer is a CULL list
 */
static void lObserveAddEntry(const void *pointer, const void *owner, bool is_list) {
   lObserveEntry *entry = lObserveMallocEntry();

   entry->pointer = pointer;
   entry->owner = owner;
   entry->is_list = is_list;
   entry->direct_access = OBSERVE_READ;
   entry->indirect_access = OBSERVE_READ;
   entry->is_master_list = false;

   pthread_mutex_lock(&ob_mtx);
   sge_htable_store(ob_ht, &pointer, entry);
   pthread_mutex_unlock(&ob_mtx);
} 

/*
 * @brief Add a pointer and its binding CULL element to be observed
 *
 * @param[in] pointer Pointer to a CULL element or list
 * @param[in] owner   Pointer to the owner of pointer (The binding CULL element or list)
 * @param[in] is_list true if pointer is a CULL list
 */
void lObserveAdd(const void *pointer, const void *owner, bool is_list) {
   pthread_mutex_lock(&ob_mtx);
   lObserveEntry *entry = nullptr;
   int found = sge_htable_lookup(ob_ht, &pointer, (const void **) &entry);
   if (found == False) {
      pthread_mutex_unlock(&ob_mtx);
      lObserveAddEntry(pointer, owner, is_list);
   } else {
      pthread_mutex_unlock(&ob_mtx);
   }
}

/*
 * @brief Removed a pointer from the observation.
 */
void lObserveRemove(const void *pointer) {
   pthread_mutex_lock(&ob_mtx);
   lObserveEntry *entry = nullptr;
   int found = sge_htable_lookup(ob_ht, &pointer, (const void **) &entry);

   if (found == True) {
      sge_free(&entry);
      sge_htable_delete(ob_ht, &pointer);
   } else {
      fprintf(stderr, "ERROR: lObserveRemove %p not found\n", pointer);
   }
   pthread_mutex_unlock(&ob_mtx);
}

/*
 * @brief Allows to tag a registered CULL list as master list with a name used for output
 */
void lObserveChangeListType(const void *pointer, bool is_master_list, const char *list_name) {
   pthread_mutex_lock(&ob_mtx);
   lObserveEntry *entry = nullptr;
   int found = sge_htable_lookup(ob_ht, &pointer, (const void **) &entry);

   if (found == True) {
      entry->list_name = list_name;
      entry->is_master_list = is_master_list;

      dstring *ob_dstring = cull_state_get_observe_dstring();
      sge_dstring_sprintf_append(ob_dstring, "%s(%s|%s)\n", list_name, "RO", "RO");
   } else {
      fprintf(stderr, "ERROR: lObserveChangeListType %p not found\n", pointer);
   }
   pthread_mutex_unlock(&ob_mtx);
}

/*
 * @brief Allows to change the ownership details of entries.
 *
 * Is sometimes called twice. Once to notify about removal of ownership (when a list or element 
 * changes from bound to unbount state. In this case new_owner is nullptr and old_owner points to
 * the element that previously owned the elemnt.
 * Called a second time to make decalre the new owner (change form unbound to bound state)
 *
 * @param[in] pointer   CULL elem or list where the ownership changes
 * @param[in] new_owner New Owner of pointer (or nullptr if element is unbound afterwards)
 * @param[in] old_owner Pointer to the binding CULL element or list (or nullptr if the element was unbound before)
 * @param[in] nm        CULL nm if the attribute name is available otherwise NoName.
 */
void lObserveChangeOwner(const void *pointer, const void *new_owner, const void *old_owner, int nm) {
   DENTER(OBSERVE_LAYER);

   const void *owner = nullptr;
   if (new_owner != nullptr) {
      owner = new_owner;
   } else if (old_owner != nullptr) {
      owner = old_owner;
   } else {
      fprintf(stderr, "ERROR lObserveChangeOwner both pinter are nullptr");
   }

   pthread_mutex_lock(&ob_mtx);
   lObserveEntry *entry = nullptr;
   int found = sge_htable_lookup(ob_ht, &pointer, (const void **) &entry);
   if (found == True) {
      entry->owner = new_owner;
   } 
   pthread_mutex_unlock(&ob_mtx);

   lObserveChangeValue(pointer, true, nm);
   lObserveChangeValue(owner, false, nm);
   DRETURN_VOID;
}

void lObserveSwitchOwner(const void *pointer_a, const void *pointer_b, const void *owner_a, const void *owner_b, int nm) {
   DENTER(OBSERVE_LAYER);

   pthread_mutex_lock(&ob_mtx);
   lObserveEntry *entry_a = nullptr;
   int found = sge_htable_lookup(ob_ht, &pointer_a, (const void **) &entry_a);
   if (found == True) {
      entry_a->owner = owner_b;
   } 
   lObserveEntry *entry_b = nullptr;
   found = sge_htable_lookup(ob_ht, &pointer_b, (const void **) &entry_b);
   if (found == True) {
      entry_b->owner = owner_a;
   } 
   pthread_mutex_unlock(&ob_mtx);

   lObserveChangeValue(pointer_a, true, nm);
   lObserveChangeValue(pointer_b, true, nm);
   if (owner_a) {
      lObserveChangeValue(owner_a, false, nm);
   }
   if (owner_b) {
      lObserveChangeValue(owner_b, false, nm);
   }
   DRETURN_VOID;
}

/*
 * @brief Used to record changes for a pointer.
 *
 * @param[in] pointer  CULL elem or list where the ownership changes
 * @param[in] has_hash Has to be set to true if nm is an attribute where a htable exists
 * @param[in] nm       Attribute which is changed. 
 */
void lObserveChangeValue(const void *pointer, bool has_hash, int nm) {
   DENTER(OBSERVE_LAYER);

   pthread_mutex_lock(&ob_mtx);
   dstring *ob_dstring = cull_state_get_observe_dstring();
   dstring msg = DSTRING_INIT;
   bool show = false;
   int level = 0;
   bool is_first = true;
   bool is_last_found_a_master = false;

   // Walk upward the object tree beginning from the object that changed to the list that contains the object
   lObserveEntry *previous_entry = nullptr;
   lObserveEntry *entry = nullptr;
   int found = sge_htable_lookup(ob_ht, &pointer, (const void **) &entry);
   while (found) {
      bool changed = false;

      // Found the next element or list
      if (found == True) {
         if (level == 0) {
            // First level: where the change happens. Remember that RW did happen.
            if (entry->direct_access != OBSERVE_WRITE) {
               changed = true;
               entry->direct_access = OBSERVE_WRITE;
            }
         } else if (level == 1) {
            // Second level: Parent of the changed object. If there is a hashtable
            // on the attribute then remember the write access to that hashtable
            if (entry->direct_access != OBSERVE_WRITE && has_hash) {
               changed = true;
               entry->direct_access = OBSERVE_WRITE;
            }
            // Changing child will update the RW only as indirect_access.
            if (entry->indirect_access != OBSERVE_WRITE) {
               changed = true;
               entry->indirect_access = OBSERVE_WRITE;
            }
         } else {
            // In higer levels only the indirect_access is 'forwarded' in the dependency chain.
            if (entry->indirect_access != OBSERVE_WRITE) {
               changed = true;
               entry->indirect_access = OBSERVE_WRITE;
            }
         }

         // If there was a previous element then we try to find the primary key of that
         int primary_key_nm = NoName;
         const lDescr *descr = nullptr;
         if (previous_entry != nullptr) {
            if (!previous_entry->is_list) {
               descr = lGetElemDescr(previous_entry->pointer);
               for (int i = 0; descr[i].nm != NoName; i++) {
                  if (descr[i].mt & CULL_PRIMARY_KEY) {
                     primary_key_nm = descr[i].nm;
                     break;
                  }
               }
            }
         }

         // To make it human readable we:
         //    * print list names for master lists
         //    * add the primary key in brackets to identyfy which element was changed
         //    * for attribute changed we show the CULL attribute name
         //    * if this is not known then an '?' will be shown
         dstring name = DSTRING_INIT;
         if (entry->is_master_list) {
            if (primary_key_nm != NoName) {
               int pos = lGetPosInDescr(descr, primary_key_nm);
               if (mt_get_type(descr[pos].mt) == lUlongT) {
                  sge_dstring_sprintf(&name, "%s[%u]", entry->list_name, lGetUlong(previous_entry->pointer, primary_key_nm));
               } else if (mt_get_type(descr[pos].mt) == lStringT) { 
                  sge_dstring_sprintf(&name, "%s[%s]", entry->list_name, lGetString(previous_entry->pointer, primary_key_nm));
               } else if (mt_get_type(descr[pos].mt) == lHostT) { 
                  sge_dstring_sprintf(&name, "%s[%s]", entry->list_name, lGetHost(previous_entry->pointer, primary_key_nm));
               } else {
                  sge_dstring_sprintf(&name, "%s[? %s]", entry->list_name, lNm2Str(primary_key_nm));
               }
            } else {
               sge_dstring_sprintf(&name, "%s", entry->list_name);
            }
         } else {
            if (is_first) {
               if (nm != NoName) {
                  sge_dstring_sprintf(&name, "%s", lNm2Str(nm));
               } else {
                  sge_dstring_sprintf(&name, "CULL");
               }
            } else {
               if (primary_key_nm != NoName) {
                  int pos = lGetPosInDescr(descr, primary_key_nm);
                  if (mt_get_type(descr[pos].mt) == lUlongT) {
                     sge_dstring_sprintf(&name, "[%u]", lGetUlong(previous_entry->pointer, primary_key_nm));
                  } else if (mt_get_type(descr[pos].mt) == lStringT) { 
                     sge_dstring_sprintf(&name, "[%s]", lGetString(previous_entry->pointer, primary_key_nm));
                  } else if (mt_get_type(descr[pos].mt) == lHostT) { 
                     sge_dstring_sprintf(&name, "[%s]", lGetHost(previous_entry->pointer, primary_key_nm));
                  } else {
                     sge_dstring_sprintf(&name, "[? %s]", lNm2Str(primary_key_nm));
                  }
               } else {
                  sge_dstring_sprintf(&name, "?");
               }
            }
         }

         // if the change happend in a child of a master list (or even further below) then
         // we want to display something later on
         if (entry->is_master_list) {
            is_last_found_a_master = true;
            show = changed;
         }

         // generate output for the element or list that is handled currently
         sge_dstring_sprintf_append(&msg, "%s%s(%s|%s)", is_first ? "" : " ", sge_dstring_get_string(&name),
                                    entry->direct_access == OBSERVE_WRITE ? "RW" : "RO",
                                    entry->indirect_access == OBSERVE_WRITE ? "RW" : "RO");
         sge_dstring_free(&name);
         is_first = false;
      }

      // continue by climbing up the object tree
      previous_entry = entry;
      found = sge_htable_lookup(ob_ht, &entry->owner, (const void **) &entry);
      level++;
   }

   // We are not interested in changes that are done in elements or list that 
   // are not bound in a master list
   if (is_last_found_a_master && (show || show_all)) {
      sge_dstring_sprintf_append(ob_dstring, "%s\n", sge_dstring_get_string(&msg));
   }
   sge_dstring_free(&msg);
   pthread_mutex_unlock(&ob_mtx);
   DRETURN_VOID;
}

#endif
