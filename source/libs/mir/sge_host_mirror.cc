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
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/ocs_MatchCache.h"
#include "sgeobj/ocs_Matcher.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HM.h"

#include "mir/sge_mirror.h"
#include "mir/sge_host_mirror.h"

/**
 * @brief Update the master hostlists
 *
 * Update the global master lists of hosts
 * based on an event.
 * The function is called from the event mirroring interface.
 * Updates the execution host list or the list of configurations
 * depending on the event received; the element is found by the
 * host name key of the event's object type.
 *
 * @param evc the event client the event arrived on
 * @param type event type
 * @param action action to perform
 * @param event the event; its object list is taken over into the master list
 * @param clientdata client data
 *
 * @return SGE_EMA_OK if the update is successful, else SGE_EMA_FAILURE
 *
 * @note The function should only be called from the event mirror interface.
 *
 * @see `sge_mirror_update_master_list()`, `ocs_mirror_update_master_list_by_type()`
 */
sge_callback_result
host_update_master_list([[maybe_unused]] sge_evc_class_t *evc, const sge_object_type type,
                        const sge_event_action action, const ocs::tom::Element<ocs::tom::ET> event,
                        [[maybe_unused]] void *clientdata) {
   DENTER(TOP_LAYER);

   if (ocs_mirror_update_master_list_by_type(type, action, event) != SGE_EM_OK) {
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
 * The generic merge is left untouched. It is reached from several mirrors and
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
 * @param event the event; its object list is taken over into the master list
 * @param clientdata client data
 *
 * @return SGE_EMA_OK, or SGE_EMA_FAILURE if the merge failed
 *
 * @note The function should only be called from the event mirror interface.
 *
 * @see `host_update_master_list()`, `ocs::MatchCache::detach()`, `ocs::MatchCache::adopt()`
 */
sge_callback_result
hgroup_update_master_list([[maybe_unused]] sge_evc_class_t *evc, const sge_object_type type,
                          const sge_event_action action, const ocs::tom::Element<ocs::tom::ET> event,
                          [[maybe_unused]] void *clientdata) {
   DENTER(TOP_LAYER);

   // the master list stays untyped: the generic merge below replaces it through the pointer
   lList **list = ocs::DataStore::get_master_list_rw(type);
   const char *key = ocs::tom::get<ET_strkey>(event);

   // Both versions are in hand before the merge, which is what lets the decision
   // be made without a version field. On every other action either the old or
   // the new element is absent, and then there is nothing to carry.
   //
   // A group holding no cache is asked nothing: that is every group of a cluster
   // that configures no matcher, and every group whose hosts have not yet made a
   // request, so the recomputation that follows an execution host movement stays
   // as cheap as it is today.
   //
   // The master list is the host group list; the event carries host groups (HGRP_Type).
   const auto before = (action == SGE_EMA_MOD || action == SGE_EMA_DEL)
                       ? ocs::tom::find_first_rw<HGRP_name>(*list, key) : ocs::tom::Element<ocs::tom::HGRP>();
   const auto after = ocs_mirror_event_objects<ocs::tom::HGRP>(event).front();
   const bool matchers_changed =
           action == SGE_EMA_MOD && before && !ocs::Matcher::cache_carries_over(before, after);
   const bool carry = action == SGE_EMA_MOD && before && !matchers_changed &&
                      static_cast<bool>(ocs::tom::get<HGRP_match_cache>(before));
   ocs::tom::ListPtr<ocs::tom::HM> cache;

   /*
    * CS-2690. A matcher that is taken away has to stop admitting, by every
    * route - and the cache holding the admission does not sit on the group that
    * changed. A cache belongs to the group that was ASKED: the permission check
    * asks "@admin_hosts", the walk behind it descends into whatever groups that
    * one references, and the entry is written where the question was put. Change
    * the matcher in the referenced group and the event arrives for THAT group,
    * while the stale entry sits on the other one.
    *
    * Measured, not reasoned about: with the matcher directly in "@admin_hosts"
    * the revocation worked and was verified in CS-2685; through a referenced
    * group the host stayed an administrative host after the matcher was gone.
    * The integration test of this package is what surfaced it.
    *
    * So every cache goes whenever a matcher set changes, and likewise when a
    * group that carried matchers is deleted. An addition needs none of this: a
    * cache holds positive results only, so a new matcher can widen what is
    * admitted but cannot make a stored entry wrong.
    */
   bool deleted_a_carrier = false;

   if (action == SGE_EMA_DEL && before) {
      // only whether the group carried a matcher counts; the split lists are freed right away
      deleted_a_carrier = std::get<0>(hgroup_split_members(before));
   }
   const bool drop_everything = matchers_changed || deleted_a_carrier;

   if (carry) {
      // out of the way of the merge, which frees the old element
      cache = ocs::MatchCache::detach(before);
   }

   // the merge finds the element by the type's key field, HGRP_name, as the lookups here do
   if (ocs_mirror_update_master_list_by_type(type, action, event) != SGE_EM_OK) {
      // a detached cache is freed with its handle
      DRETURN(SGE_EMA_FAILURE);
   }

   if (carry) {
      // the pruning that goes with the move needs the resolved membership of the
      // new element, so it happens here and not above
      ocs::MatchCache::adopt(ocs::tom::find_first_rw<HGRP_name>(*list, key), std::move(cache));
   }

   if (drop_everything) {
      ocs::MatchCache::discard_all(ocs::tom::List<ocs::tom::HGRP>::from_typed(*list));
   }

   DRETURN(SGE_EMA_OK);
}
