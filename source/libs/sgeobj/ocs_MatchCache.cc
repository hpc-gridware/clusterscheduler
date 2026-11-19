/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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
 *  See the License for the specific provisions governing your rights and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief The cache of hosts a matcher of a host group has already admitted
 *
 * @see ocs_MatchCache.h
 */

#include <mutex>
#include <utility>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/cull/sge_hgroup_HGRP_L.h"
#include "sgeobj/cull/sge_href_HR_L.h"
#include "sgeobj/cull/sge_matchcache_HM_L.h"
#include "sgeobj/ocs_MatchCache.h"
#include "sgeobj/sge_conf.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HM.h"
#include "sgeobj/tom/ocs_tom_HR.h"

/**
 * @brief The one lock that guards every structural change of a matching cache
 *
 * One for all caches rather than one per host group, and the reason is lifetime
 * rather than contention. The frequent path never takes it - a hit is a hash
 * access and an atomic store - and the only other writer is a single mirror
 * thread per data store, working events one at a time, so a per-group lock would
 * buy no parallelism there at all. A per-group lock, on the other hand, would
 * have nowhere to live: the object it belongs to is replaced and freed on every
 * event merge, so the lock would have to be carried across that merge, and the
 * carrying would itself need a lock.
 *
 * It cannot ride on the read lock of the data store, which lets several threads
 * in at once, and it must not demand the write lock, which the check path does
 * not hold and must not take. Lock order is the same in both threads: data store
 * lock first, this one second.
 *
 * The rule holds **without exception**, including where a structural change
 * happens while the write lock of the data store is already held, as it does on
 * the event merge. Making that case distinction would save an uncontended lock
 * and cost the certainty that the reasoning holds everywhere.
 *
 * If contention is ever measured, the answer is striping this into a static
 * array chosen by a hash of the group name - which keeps the lifetime question
 * closed, because the array is static.
 */
static std::mutex match_cache_mutex;

/// How many entries one insertion examines for expiry; fixed, small, not configurable
static constexpr int MATCH_CACHE_RECLAIM_BOUND = 8;

/// How many entries one carry-over examines; a relief valve, not an economy
static constexpr int MATCH_CACHE_PRUNE_BOUND = 64;

/**
 * @brief Drop expired entries, examining a bounded number of them
 *
 * Reclamation is amortised at insertion and never a pass over the whole cache:
 * whoever writes an entry also examines a few and drops what has expired. That
 * bounds the cache by construction rather than by a timer - it can only grow
 * through an insertion, and every insertion also reclaims, so where insertions
 * stop, growth stops with them.
 *
 * A periodic sweeper is not merely superfluous here, it is unavailable: the
 * timed event thread reaches the main data store only, while the store this
 * cache fills is fed by a mirror thread that waits for events without a timeout.
 *
 * The examination starts at the front, and that fits the case the expiry exists
 * for. Entries are appended at the back, so the front holds the oldest - and the
 * case expiry is about is a fleet that does not reuse instance names, where the
 * old entries are exactly the ones whose hosts are gone and which are therefore
 * never refreshed.
 *
 * The time of last use is read with the atomic accessor, as concurrent lookups write it without a lock.
 *
 * @param cache the matching cache (HM_Type), already guarded by the caller; not an empty handle
 * @param now the current time as a 64 bit timestamp, that is in microseconds
 */
static void
match_cache_reclaim(const ocs::tom::List<ocs::tom::HM> cache, const uint64_t now) {
   DENTER(TOP_LAYER);

   const int ttl_seconds = mconf_get_matcher_cache_time();

   if (ttl_seconds <= 0) {
      // 0 means an entry does not expire
      DRETURN_VOID;
   }

   // the time of last use is a 64 bit timestamp, which in this product means
   // microseconds; the configured period is in seconds
   const uint64_t ttl = sge_gmt32_to_gmt64(static_cast<uint32_t>(ttl_seconds));

   auto entry = cache.front();

   for (int examined = 0; entry && examined < MATCH_CACHE_RECLAIM_BOUND; examined++) {
      const auto next = entry.next();

      if (now > ocs::tom::get_atomic<HM_last_used>(entry) + ttl) {
         cache.erase(entry);
      }
      entry = next;
   }

   DRETURN_VOID;
}

/**
 * @brief Has this host already been admitted by a matcher of this group?
 *
 * The lookup in front of the matcher walk, and a hash access because the host
 * name is the hashed key of the entry.
 *
 * A hit advances the time of last use, so **this is not a read path**: a
 * successful lookup modifies the data store. That write is atomic and takes no
 * lock - it carries no invariant with any other field, and several threads
 * writing "now" produce competing values every one of which is a valid recent
 * time of use. Only a structural change takes the lock.
 *
 * @param hgroup the group to ask; an empty handle answers no
 * @param hostname the host in question, already resolved by the caller
 *
 * @return true if the host has an unexpired entry
 */
bool
ocs::MatchCache::lookup(const ocs::tom::Element<ocs::tom::HGRP> hgroup, const char *hostname) {
   DENTER(TOP_LAYER);

   if (!hgroup || hostname == nullptr) {
      DRETURN(false);
   }

   const auto entry = ocs::tom::find_first_rw<HM_name>(ocs::tom::get<HGRP_match_cache>(hgroup), hostname);

   if (!entry) {
      DRETURN(false);
   }

   ocs::tom::set_atomic<HM_last_used>(entry, sge_get_gmt64());

   DRETURN(true);
}

/**
 * @brief Record that a matcher of this group has admitted this host
 *
 * Called only after the walk has answered yes. **Only positive results are
 * cached**: a rejected host creates no entry, so the key space of the cache
 * cannot be determined from outside. The price is that a rejected host runs the
 * walk on every one of its requests, which is the only path here whose frequency
 * is set by someone other than the administrator.
 *
 * @param hgroup the group that admitted the host; an empty handle changes nothing
 * @param hostname the admitted host, already resolved by the caller
 *
 * @see #match_cache_reclaim
 */
void
ocs::MatchCache::insert(const ocs::tom::Element<ocs::tom::HGRP> hgroup, const char *hostname) {
   DENTER(TOP_LAYER);

   if (!hgroup || hostname == nullptr) {
      DRETURN_VOID;
   }

   const uint64_t now = sge_get_gmt64();

   std::lock_guard<std::mutex> guard(match_cache_mutex);
   auto cache = ocs::tom::get<HGRP_match_cache>(hgroup);
   ocs::tom::ListPtr<ocs::tom::HM> created;

   if (!cache) {
      created = ocs::tom::ListPtr<ocs::tom::HM>::create("");
      cache = *created;
   }
   match_cache_reclaim(cache, now);

   // Another thread may have inserted the same host while we walked. The key is
   // unique and hashed, and a list that ever holds two entries under one key
   // cannot be repaired: removing either drops the key from the hash and leaves
   // the other present but unfindable.
   if (!ocs::tom::find_first<HM_name>(cache, hostname)) {
      const auto fresh = cache.emplace_back<HM_name>(ocs::tom::Host(hostname));

      ocs::tom::set<HM_last_used>(fresh, now);
   }
   if (created) {
      ocs::tom::set<HGRP_match_cache>(hgroup, std::move(created));
   }

   DRETURN_VOID;
}

/**
 * @brief Take the matching cache out of a group that is about to be replaced
 *
 * Half of the move across an event merge. It is two halves rather than one call
 * because the generic merge frees the old element before the new one is in
 * place, so nothing holds both at the same time - and because the pruning the
 * other half does needs the resolved membership of the *new* element.
 *
 * The returned list is **detached**: it hangs on no object any more, so the
 * caller may free it without this lock. That is the failure path, and it is the
 * only place a cache is touched outside the lock.
 *
 * @param hgroup the group the cache is taken from; an empty handle gives an empty handle
 *
 * @return the cache, owned by the caller, or an empty handle if the group carried none
 *
 * @see #adopt
 */
ocs::tom::ListPtr<ocs::tom::HM>
ocs::MatchCache::detach(const ocs::tom::Element<ocs::tom::HGRP> hgroup) {
   DENTER(TOP_LAYER);

   if (!hgroup) {
      DRETURN(nullptr);
   }

   std::lock_guard<std::mutex> guard(match_cache_mutex);
   DRETURN(hgroup.exchange<HGRP_match_cache>(nullptr));
}

/**
 * @brief Give a detached matching cache to the group that replaces its owner
 *
 * The other half of the move, and the place the cache prunes itself.
 *
 * **Every entry whose host appears in the resolved membership of the new object
 * is dropped.** Such an entry is not merely stale, it is *unreachable*: layer 3
 * is entered only when layer 2 has not already said yes, so a host present in
 * layer 2 never reaches this cache again. That is aimed exactly at what a
 * roll-out produces - a host gets its entry on its first request, a few moments
 * later it has become an execution host, and the entry goes. What remains in the
 * long run is precisely the population the cache exists for: hosts admitted
 * through a matcher that are not execution hosts.
 *
 * **The direction of the comparison is part of the rule.** The iteration goes
 * over the cache and looks each entry up in the resolved membership, which is
 * hashed by host name. The opposite direction, or forming the difference between
 * the old and the new resolved membership, would mean a pass over the large list
 * inside the write lock of the data store - the very cost this avoids. The
 * iteration is bounded as well, not as an economy but as a relief valve, so that
 * a cache temporarily grown large during a roll-out burst cannot hold the lock.
 *
 * @param hgroup the group that receives the cache; with an empty handle the cache is freed
 * @param cache the detached cache (HM_Type); taken over, whatever the outcome
 *
 * @see #detach
 */
void
ocs::MatchCache::adopt(const ocs::tom::Element<ocs::tom::HGRP> hgroup, ocs::tom::ListPtr<ocs::tom::HM> &&cache) {
   DENTER(TOP_LAYER);

   auto list = std::move(cache);

   if (!list || !hgroup) {
      // a cache without a group to receive it is freed with the handle
      DRETURN_VOID;
   }

   std::lock_guard<std::mutex> guard(match_cache_mutex);
   const ocs::tom::ConstList<ocs::tom::HR> resolved = ocs::tom::get<HGRP_cached_hosts>(hgroup);
   auto entry = list->front();

   for (int examined = 0; entry && examined < MATCH_CACHE_PRUNE_BOUND; examined++) {
      const auto next = entry.next();

      if (ocs::tom::find_first<HR_name>(resolved, ocs::tom::get<HM_name>(entry))) {
         list->erase(entry);
      }
      entry = next;
   }

   // Exchange rather than set: the new element carries an empty cache of its own,
   // because the field does not travel in an event payload, and that one is freed
   // with the handle below, still under the lock.
   const auto replaced = hgroup.exchange<HGRP_match_cache>(std::move(list));

   DRETURN_VOID;
}

/**
 * @brief Throw away the matching cache of every host group
 *
 * A cache belongs to the group that was **asked**, not to the group whose
 * matcher answered. The walk behind it descends through group references, so the
 * entries of `@admin_hosts` may rest on a matcher three groups away - and when
 * that matcher changes, the object that changed is not the object holding the
 * cache. Nothing in the event for the one reaches the other.
 *
 * Hence the blunt rule: when any group's matcher set changes, every cache goes.
 * The alternative is to compute, at merge time, which groups can reach the one
 * that changed - and to do it against a master list in which the other groups
 * may or may not have been merged yet, since the order of events across objects
 * is not guaranteed. A rule whose correctness depends on that ordering is a rule
 * nobody can check.
 *
 * The cost is a re-learn, and it is paid only on an administrative change to a
 * matcher, never on a request. What it buys is that an admission cannot outlive
 * the matcher that granted it, by any route - which is the one property of this
 * design that must not be argued, only guaranteed.
 *
 * @param master_hgroup_list the host groups of one data store; may be an empty handle
 *
 * @see #carry_over, ocs::Matcher::cache_carries_over
 */
void
ocs::MatchCache::discard_all(const ocs::tom::List<ocs::tom::HGRP> master_hgroup_list) {
   DENTER(TOP_LAYER);

   std::lock_guard<std::mutex> guard(match_cache_mutex);

   for (const auto hgroup : master_hgroup_list) {
      // the cache taken out is freed with the handle, still under the lock
      static_cast<void>(hgroup.exchange<HGRP_match_cache>(nullptr));
   }

   DRETURN_VOID;
}

