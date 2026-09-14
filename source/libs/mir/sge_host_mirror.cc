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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Mirroring the host lists
 *
 * Admin, submit and execution hosts are three separate master lists fed by one
 * callback, which picks the list from the event type. Host groups are keyed by
 * host name too and shared that callback until CS-2680; they now have one of
 * their own, because their matching cache has to survive the merge.
 *
 * @see sge_host_mirror.h
 * @see sge_mirror.h
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_hgroup_HGRP_L.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_MatchCache.h"
#include "sgeobj/ocs_Matcher.h"

#include "mir/sge_mirror.h"
#include "mir/sge_host_mirror.h"

/**
 * @brief Update the master hostlists
 *
 * Update the global master lists of hosts
 * based on an event.
 * The function is called from the event mirroring interface.
 * Updates admin, submit or execution host list depending
 * on the event received.
 *
 * @param evc the event client the event arrived on
 * @param type event type
 * @param action action to perform
 * @param event the raw event
 * @param clientdata client data
 *
 * @return true, if update is successful, else false
 *
 * @note The function should only be called from the event mirror interface.
 *
 * @see `sge_mirror_update_master_list()`, `sge_mirror_update_master_list_host_key()`
 */
sge_callback_result
host_update_master_list(sge_evc_class_t *evc, sge_object_type type,
                        sge_event_action action, lListElem *event, void *clientdata) {
   DENTER(TOP_LAYER);

   lList **list;
   const lDescr *list_descr;
   int     key_nm;

   const char *key;

   list = ocs::DataStore::get_master_list_rw(type);
   list_descr = lGetListDescr(lGetList(event, ET_new_version));
   key_nm = object_type_get_key_nm(type); 

   key = lGetString(event, ET_strkey);

   if (sge_mirror_update_master_list_host_key(list, list_descr, key_nm, key, 
                                              action, event) != SGE_EM_OK) {

      DRETURN(SGE_EMA_FAILURE);
   }

   DRETURN(SGE_EMA_OK);
}

/**
 * @brief Update the master host group list, keeping the matching cache alive
 *
 * Host groups are mirrored like the three host lists - same key type, same
 * generic merge - with one addition: a group may carry a **matching cache**, the
 * hosts a matcher of that group has already admitted (CS-2680). That cache is
 * process-local derived state. It is not spooled and does not travel in an event
 * payload, so the element arriving with the event carries an empty one; without
 * this callback every event for a host group would silently throw the cache away
 * and the matcher walk would start again from nothing.
 *
 * Throwing it away is also what **must** happen when the matchers change, and
 * that is the reason the decision is asked rather than assumed. A hit in the
 * cache *confers* - it answers the permission question with yes without
 * consulting a matcher - so the completeness of its invalidation is a security
 * property, not a question of performance. The question is put to
 * `ocs::Matcher::cache_carries_over()`, which has both versions in hand at once
 * and therefore needs no version field; it answers yes exactly when the two
 * carry the same set of matchers. That is complete, because every change to the
 * definition of a group arrives as an event for that object, and precise,
 * because the recomputation that follows every execution host movement leaves
 * the matchers alone and must not make the cache cold.
 *
 * The generic merge is left untouched. It is reached from four mirrors and nine
 * call sites, and it frees the old element before the new one is in place, so
 * the cache is taken aside before the call and handed over after it.
 *
 * On `SGE_EMA_LIST` nothing is carried: that action swaps the whole master list,
 * so every old element dies at once, and matching them up by name would be a
 * second mechanism for a case that occurs on initial synchronisation. The cost
 * is a re-learn, which is what the cache is built to do.
 *
 * @param evc the event client the event arrived on
 * @param type event type; always `SGE_TYPE_HGROUP` here
 * @param action action to perform
 * @param event the raw event
 * @param clientdata client data
 *
 * @return SGE_EMA_OK, or SGE_EMA_FAILURE if the merge failed
 *
 * @note The function should only be called from the event mirror interface.
 *
 * @see `host_update_master_list()`, `ocs::MatchCache::detach()`, `ocs::MatchCache::adopt()`
 */
sge_callback_result
hgroup_update_master_list(sge_evc_class_t *evc, sge_object_type type,
                          sge_event_action action, lListElem *event, void *clientdata) {
   DENTER(TOP_LAYER);

   lList **list = ocs::DataStore::get_master_list_rw(type);
   const lDescr *list_descr = lGetListDescr(lGetList(event, ET_new_version));
   const int key_nm = object_type_get_key_nm(type);
   const char *key = lGetString(event, ET_strkey);

   // Both versions are in hand before the merge, which is what lets the decision
   // be made without a version field. On every other action either the old or
   // the new element is absent, and then there is nothing to carry.
   //
   // A group holding no cache is asked nothing: that is every group of a cluster
   // that configures no matcher, and every group whose hosts have not yet made a
   // request, so the recomputation that follows an execution host movement stays
   // as cheap as it is today.
   lListElem *before = action == SGE_EMA_MOD ? lGetElemHostRW(*list, key_nm, key) : nullptr;
   const bool carry = before != nullptr &&
                      lGetList(before, HGRP_match_cache) != nullptr &&
                      ocs::Matcher::cache_carries_over(before, lFirst(lGetList(event, ET_new_version)));
   lList *cache = nullptr;

   if (carry) {
      // out of the way of the merge, which frees the old element
      cache = ocs::MatchCache::detach(before);
   }

   if (sge_mirror_update_master_list_host_key(list, list_descr, key_nm, key,
                                              action, event) != SGE_EM_OK) {
      lFreeList(&cache);
      DRETURN(SGE_EMA_FAILURE);
   }

   if (carry) {
      // the pruning that goes with the move needs the resolved membership of the
      // new element, so it happens here and not above
      ocs::MatchCache::adopt(lGetElemHostRW(*list, key_nm, key), &cache);
   }
   lFreeList(&cache);

   DRETURN(SGE_EMA_OK);
}
