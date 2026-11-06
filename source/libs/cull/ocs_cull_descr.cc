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
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief The descriptor registry, see ocs_cull_descr.h
 */

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <dlfcn.h>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_lerrnoP.h"
#include "cull/cull_list.h"
#include "cull/cull_multitypeP.h"
#include "cull/cull_multitype.h"
#include "cull/cull_state.h"
#include "cull/cull_stats.h"
#include "cull/msg_cull.h"
#include "cull/ocs_cull_descr.h"

static_assert(offsetof(cull_descr_block<2>, e) == sizeof(cull_descr_header),
              "the entries of a block must follow its header without a gap");

namespace {

   /** @brief The registered descriptors of the process, keyed by their hash */
   struct cull_descr_registry {
      std::mutex mtx;                                               ///< guards everything below
      std::unordered_multimap<uint64_t, cull_descr_header *> map; ///< registered descriptors by hash
      size_t soft_cap{OCS_CULL_DESCR_SOFT_CAP};                     ///< registry size above which copies are private
      bool cap_reported{false};                                     ///< the soft cap warning was logged since the last shrink
      CULL_STATS_CODE(size_t peak{0};)                              ///< largest size the map reached (statistics build)
   };

   /**
    * @brief The pointer to the one registry of the process
    *
    * The registry is allocated on first use and never destroyed, so that a
    * descriptor released by a thread that outlives the static destructors still
    * finds it. The pointer lives in the data of the object holding this code, so
    * its address names that object (N-A-22).
    *
    * @return the pointer, set to the registry
    */
   cull_descr_registry *&registry_slot() {
      static cull_descr_registry *r = new cull_descr_registry;
      return r;
   }

   /**
    * @brief The one registry of the process
    * @return the registry
    */
   cull_descr_registry &registry() {
      return *registry_slot();
   }

   /**
    * @brief The file name of the loaded object an address belongs to
    * @param addr the address
    * @return the file name, or "unknown"
    */
   std::string object_of(const void *addr) {
      Dl_info info{};
      if (addr != nullptr && dladdr(addr, &info) != 0 && info.dli_fname != nullptr) {
         return info.dli_fname;
      }
      return "unknown";
   }

   /**
    * @brief The header in front of the entries of a descriptor
    * @param dp the entries of a static or counted descriptor
    * @return its header
    */
   cull_descr_header *header_of(const lDescr *dp) {
      return reinterpret_cast<cull_descr_header *>(const_cast<lDescr *>(dp)) - 1;
   }

   /**
    * @brief The entries behind a header
    * @param h the header of a counted descriptor
    * @return its entries
    */
   lDescr *entries_of(cull_descr_header *h) {
      return reinterpret_cast<lDescr *>(h + 1);
   }

   /**
    * @brief Number of entries of a descriptor, without the terminating one
    * @param dp the entries
    * @return the count
    */
   uint32_t count_entries(const lDescr *dp) {
      uint32_t n = 0;
      while (mt_get_type(dp[n].mt) != lEndT) {
         n++;
      }
      return n;
   }

   /**
    * @brief Whether two descriptors have the same entries
    *
    * Process-local flags (#CULL_DESCR_INTERNED) are not part of the comparison.
    *
    * @param a the entries of one descriptor
    * @param b the entries of the other
    * @param n the number of entries of both, without the terminating one
    * @return true if every entry has the same field number, type and slot offset
    */
   bool same_entries(const lDescr *a, const lDescr *b, const uint32_t n) {
      for (uint32_t i = 0; i < n; i++) {
         if (a[i].nm != b[i].nm || (a[i].mt & ~CULL_DESCR_INTERNED) != (b[i].mt & ~CULL_DESCR_INTERNED) ||
             a[i].off != b[i].off) {
            return false;
         }
      }
      return true;
   }

   /**
    * @brief Number of entries of a descriptor that have a value
    * @param dp the entries
    * @param n their number, without the terminating one
    * @return the count of entries whose slot offset is not #OCS_CULL_ABSENT
    */
   uint32_t count_present(const lDescr *dp, const uint32_t n) {
      uint32_t np = 0;
      for (uint32_t i = 0; i < n; i++) {
         if (dp[i].off != OCS_CULL_ABSENT) {
            np++;
         }
      }
      return np;
   }

   /**
    * @brief Size of a counted descriptor with its header and reverse map
    * @param n number of entries, without the terminating one
    * @param np number of entries that have a value
    * @return the bytes to allocate
    */
   size_t counted_size(const uint32_t n, const uint32_t np) {
      return sizeof(cull_descr_header) + (n + 1) * sizeof(lDescr) + (np < n ? np * sizeof(int16_t) : 0);
   }

   /**
    * @brief Allocate a counted copy of a descriptor, holding one reference
    *
    * With absent fields the copy gets the reverse map from slot to position
    * behind its terminating entry (N-B-6).
    *
    * @param dp the entries to copy
    * @param n their number, without the terminating one
    * @param hash their hash
    * @return the header of the copy, or nullptr if out of memory
    */
   cull_descr_header *alloc_counted(const lDescr *dp, const uint32_t n, const uint64_t hash) {
      const uint32_t np = count_present(dp, n);
      void *mem = sge_malloc(counted_size(n, np));
      if (mem == nullptr) {
         return nullptr;
      }
      auto *h = new (mem) cull_descr_header{1, n, np, hash};
      lDescr *e = entries_of(h);
      memcpy(e, dp, (n + 1) * sizeof(lDescr));
      e[0].mt |= CULL_DESCR_INTERNED;
      if (np < n) {
         auto *rmap = reinterpret_cast<int16_t *>(e + n + 1);
         for (uint32_t pos = 0; pos < n; pos++) {
            if (e[pos].off != OCS_CULL_ABSENT) {
               rmap[pos + e[pos].off] = static_cast<int16_t>(pos);
            }
         }
      }
      return h;
   }

   /**
    * @brief Free a counted descriptor whose last reference is gone
    * @param h its header
    */
   void free_counted(cull_descr_header *h) {
      h->~cull_descr_header();
      sge_free(&h);
   }

   /**
    * @brief Take a reference unless the count already dropped to zero
    *
    * A descriptor at zero is being freed by the thread that released it last and
    * must not be revived.
    *
    * @param h the header
    * @return true if a reference was taken
    */
   bool ref_if_alive(cull_descr_header *h) {
      uint32_t refs = h->refs.load(std::memory_order_relaxed);
      while (refs != 0) {
         if (h->refs.compare_exchange_weak(refs, refs + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
            return true;
         }
      }
      return false;
   }

   /// the static block of each type of a name space, by the field number of its first field
   using StaticIndex = std::unordered_map<int, const lDescr *>;

   /** @brief Immutable {name space, index} pair, published as one unit */
   struct StaticCacheEntry {
      const lNameSpace *ns;   ///< the name space this entry resolves
      const StaticIndex *idx; ///< its build-once index
   };

   std::mutex static_index_mtx;                                                       ///< guards static_index_map during a build
   std::unordered_map<const lNameSpace *, std::unique_ptr<const StaticIndex>> static_index_map; ///< per name space indices
   std::atomic<const StaticCacheEntry *> static_last{nullptr};                        ///< last used pair, lock-free fast path

   /**
    * @brief The build-once index of the static blocks of a name space
    *
    * Built like the name index of cull_name.cc: the mutex is taken only on the
    * first lookup for a name space, later lookups read one atomic pointer to an
    * immutable pair. A replaced pair is leaked on purpose, at most one per name
    * space, because a concurrent reader may still use it.
    *
    * @param ns the name space
    * @return its index
    */
   const StaticIndex *static_index(const lNameSpace *ns) {
      if (const StaticCacheEntry *cached = static_last.load(std::memory_order_acquire);
          cached != nullptr && cached->ns == ns) {
         return cached->idx;
      }
      std::lock_guard<std::mutex> guard(static_index_mtx);
      auto &slot = static_index_map[ns];
      if (!slot) {
         auto idx = std::make_unique<StaticIndex>();
         for (const lNameSpace *e = ns; e->lower != 0; ++e) {
            if (e->descr != nullptr && mt_get_type(e->descr[0].mt) != lEndT) {
               idx->emplace(e->descr[0].nm, e->descr);
            }
         }
         slot = std::move(idx);
      }
      const StaticIndex *idx = slot.get();
      static_last.store(new StaticCacheEntry{ns, idx}, std::memory_order_release);
      return idx;
   }

   /**
    * @brief The static block of the type a descriptor starts with
    * @param dp the entries of a descriptor
    * @return the static block registered in the name space of the calling thread
    *         for the type of entry 0, or nullptr if there is none
    */
   const lDescr *static_block_for(const lDescr *dp) {
      const lNameSpace *ns = cull_state_get_name_space();
      if (ns == nullptr) {
         return nullptr;
      }
      const StaticIndex *idx = static_index(ns);
      auto it = idx->find(dp[0].nm);
      return it == idx->end() ? nullptr : it->second;
   }

} // namespace

/**
 * @brief Whether a descriptor is reference counted
 * @param dp the entries of a descriptor, or nullptr
 * @return true for a counted descriptor, false for a static one, one without header and nullptr
 */
bool ocs_cull_descr_is_counted(const lDescr *dp) {
   return dp != nullptr && (dp[0].mt & CULL_DESCR_INTERNED) != 0;
}

/**
 * @brief The static block of the type whose first field is a given one
 * @param nm the field number of the first field of the type
 * @return the block registered in the name space of the calling thread, or
 *         nullptr if there is no name space or no such type
 */
const lDescr *ocs_cull_descr_static_for(const int nm) {
   const lNameSpace *ns = cull_state_get_name_space();
   if (ns == nullptr) {
      return nullptr;
   }
   const StaticIndex *idx = static_index(ns);
   auto it = idx->find(nm);
   return it == idx->end() ? nullptr : it->second;
}

/**
 * @brief The static block of the type a field belongs to
 *
 * Field numbers are allocated in one block per type, so the name space entry
 * whose range contains the field names its type, and the field sits at the
 * offset from the lower bound of the range.
 *
 * @param nm the field number
 * @param pos set to the position of the field in the block
 * @return the block registered in the name space of the calling thread, or
 *         nullptr if no type of it contains the field
 */
const lDescr *ocs_cull_descr_static_containing(const int nm, int *pos) {
   const lNameSpace *ns = cull_state_get_name_space();
   if (ns == nullptr) {
      return nullptr;
   }
   for (const lNameSpace *e = ns; e->lower != 0; ++e) {
      if (e->descr != nullptr && nm >= e->lower && nm < e->lower + e->size) {
         const int k = nm - e->lower;
         if (e->descr[k].nm == nm) {
            *pos = k;
            return e->descr;
         }
         return nullptr;
      }
   }
   return nullptr;
}

/**
 * @brief Whether a descriptor is the static block of its type
 * @param dp the entries of a descriptor, or nullptr
 * @return true if dp is the block registered for its type in the name space of the calling thread
 */
bool ocs_cull_descr_is_static(const lDescr *dp) {
   return dp != nullptr && !ocs_cull_descr_is_counted(dp) && static_block_for(dp) == dp;
}

/**
 * @brief The shared descriptor equal to a given one, with a reference for the caller
 *
 * Looks up the registry for a descriptor with the same entries and takes a
 * reference on it. On a miss a counted copy is registered. Above the soft cap the
 * copy stays private: it is reference counted like a registered one but never
 * found by a later lookup, and a warning is logged once. The argument is left to
 * the caller; it may be static, counted or a plain array without header.
 *
 * @param dp the entries of the descriptor, terminated by #lEndT
 * @return the shared or private descriptor holding one reference for the caller,
 *         or nullptr if dp is nullptr or memory is exhausted
 */
const lDescr *ocs_cull_descr_intern(const lDescr *dp) {
   DENTER(CULL_LAYER);

   if (dp == nullptr) {
      DRETURN(nullptr);
   }

   const uint32_t n = count_entries(dp);
   const uint64_t hash = ocs_cull_descr_hash(dp, n);
   cull_descr_registry &reg = registry();
   cull_descr_header *fresh = nullptr;
   size_t cap;
   {
      std::lock_guard<std::mutex> lock(reg.mtx);

      auto [first, last] = reg.map.equal_range(hash);
      for (auto it = first; it != last; ++it) {
         cull_descr_header *h = it->second;
         if (h->n == n && same_entries(entries_of(h), dp, n)) {
            if (ref_if_alive(h)) {
               CULL_STATS_TYPE(dp[0].nm, DESCR_INTERN_HIT, 1);
               DRETURN(entries_of(h));
            }
            // dead, its releasing thread frees it; replace the entry
            fresh = alloc_counted(dp, n, hash);
            if (fresh == nullptr) {
               DRETURN(nullptr);
            }
            it->second = fresh;
            CULL_STATS_TYPE(dp[0].nm, DESCR_INTERN_NEW, 1);
            CULL_STATS_TYPE(dp[0].nm, DESCR_INTERN_NEW_B, counted_size(n, count_present(dp, n)));
            DRETURN(entries_of(fresh));
         }
      }

      fresh = alloc_counted(dp, n, hash);
      if (fresh == nullptr) {
         DRETURN(nullptr);
      }
      if (reg.map.size() < reg.soft_cap) {
         reg.map.emplace(hash, fresh);
         CULL_STATS_CODE(reg.peak = std::max(reg.peak, reg.map.size());)
         CULL_STATS_TYPE(dp[0].nm, DESCR_INTERN_NEW, 1);
         CULL_STATS_TYPE(dp[0].nm, DESCR_INTERN_NEW_B, counted_size(n, count_present(dp, n)));
         DRETURN(entries_of(fresh));
      }
      CULL_STATS_TYPE(dp[0].nm, DESCR_PRIVATE, 1);
      CULL_STATS_TYPE(dp[0].nm, DESCR_PRIVATE_B, counted_size(n, count_present(dp, n)));
      if (reg.cap_reported) {
         DRETURN(entries_of(fresh));
      }
      reg.cap_reported = true;
      cap = reg.soft_cap;
   }

   // log outside the lock; the copy stays private
   WARNING(MSG_CULL_DESCR_SOFTCAPREACHED_U, static_cast<uint32_t>(cap));
   DRETURN(entries_of(fresh));
}

/**
 * @brief The descriptor a list or an element holds, with a reference for the caller
 *
 * A counted descriptor gets one more reference. A full descriptor yields the
 * static block of its type, also when it is an equal copy of it; the static
 * block is found through the name space of the calling thread. Any other
 * descriptor is interned. The argument is left to the caller.
 *
 * @param dp the entries of the descriptor, terminated by #lEndT
 * @return the descriptor to hold, or nullptr if dp is nullptr or memory is exhausted
 */
const lDescr *ocs_cull_descr_acquire(const lDescr *dp) {
   if (dp == nullptr) {
      return nullptr;
   }
   if (ocs_cull_descr_is_counted(dp)) {
      return ocs_cull_descr_ref(dp);
   }
   const lDescr *block = static_block_for(dp);
   if (block == dp) {
      return dp;
   }
   if (block != nullptr) {
      const uint32_t n = count_entries(dp);
      if (count_entries(block) == n && same_entries(block, dp, n)) {
         return block;
      }
   }
   return ocs_cull_descr_intern(dp);
}

/**
 * @brief A private counted copy of a descriptor, which the caller may still modify
 *
 * The copy is not registered, so no lookup finds it, and it holds one
 * reference for the caller. It is released with ocs_cull_descr_release().
 *
 * @param dp the entries of the descriptor, terminated by #lEndT
 * @return the copy, or nullptr if dp is nullptr or memory is exhausted
 */
const lDescr *ocs_cull_descr_private(const lDescr *dp) {
   if (dp == nullptr) {
      return nullptr;
   }
   const uint32_t n = count_entries(dp);
   cull_descr_header *h = alloc_counted(dp, n, ocs_cull_descr_hash(dp, n));
   if (h == nullptr) {
      return nullptr;
   }
   CULL_STATS_TYPE(dp[0].nm, DESCR_PRIVATE, 1);
   CULL_STATS_TYPE(dp[0].nm, DESCR_PRIVATE_B, counted_size(n, count_present(dp, n)));
   return entries_of(h);
}

/**
 * @brief The shared descriptor of a type reduced to some of its fields (CS-439)
 *
 * The one builder of reduced descriptors: the kept fields are taken in the
 * order of the type with their type and attributes, so the same field set
 * always yields the same descriptor (N-B-5). Every entry is marked
 * #CULL_IS_REDUCED unless all fields are kept, which yields the type itself.
 *
 * @param type the static block of the object type
 * @param keep one flag per field of the type, true for the fields to keep
 * @return the shared descriptor holding a reference for the caller, or nullptr
 *         if keep does not match the type, selects no field, or memory is exhausted
 */
const lDescr *ocs_cull_descr_build(const lDescr *type, const std::vector<bool> &keep) {
   DENTER(CULL_LAYER);

   const uint32_t n = type != nullptr ? count_entries(type) : 0;
   if (n == 0 || keep.size() != n) {
      LERROR(LEDESCRNULL);
      DRETURN(nullptr);
   }
   const auto kept = static_cast<uint32_t>(std::count(keep.begin(), keep.end(), true));
   if (kept == 0) {
      LERROR(LEENUMNULL);
      DRETURN(nullptr);
   }
   if (kept == n) {
      DRETURN(ocs_cull_descr_acquire(type));
   }

   std::vector<lDescr> entries;
   entries.reserve(kept + 1);
   for (uint32_t pos = 0; pos < n; pos++) {
      if (keep[pos]) {
         entries.push_back({type[pos].nm, (type[pos].mt & ~CULL_DESCR_INTERNED) | CULL_IS_REDUCED, 0});
      }
   }
   entries.push_back({NoName, lEndT | CULL_IS_REDUCED, 0});
   const lDescr *dp = ocs_cull_descr_intern(entries.data());
   if (dp == nullptr) {
      LERROR(LEMALLOC);
   }
   DRETURN(dp);
}

/**
 * @brief The shared descriptor of a type reduced to a list of its fields (CS-439, N-B-9)
 *
 * For descriptors written down in the code: the fields may be listed in any
 * order and more than once; the result is that of ocs_cull_descr_build().
 *
 * @code
 * static const lDescr *const task_descr = ocs_cull_descr_reduce(JAT_Type, {JAT_task_number, JAT_hold});
 * @endcode
 *
 * @param type the static block of the object type
 * @param fields the field numbers to keep
 * @return the shared descriptor holding a reference for the caller, or nullptr
 *         if a field does not belong to the type, none is given, or memory is exhausted
 */
const lDescr *ocs_cull_descr_reduce(const lDescr *type, const std::initializer_list<int> fields) {
   DENTER(CULL_LAYER);

   const uint32_t n = type != nullptr ? count_entries(type) : 0;
   std::vector<bool> keep(n, false);
   for (const int nm : fields) {
      const int pos = n > 0 ? nm - type[0].nm : -1;
      if (pos < 0 || static_cast<uint32_t>(pos) >= n || type[pos].nm != nm) {
         LERROR(LENAMENOT);
         DRETURN(nullptr);
      }
      keep[pos] = true;
   }
   DRETURN(ocs_cull_descr_build(type, keep));
}

/**
 * @brief Take one more reference on a descriptor
 *
 * The caller must already hold a reference, or otherwise keep the descriptor
 * alive, for the duration of the call. Static descriptors are not counted.
 *
 * @param dp the entries of a descriptor, or nullptr
 * @return dp
 */
const lDescr *ocs_cull_descr_ref(const lDescr *dp) {
   if (ocs_cull_descr_is_counted(dp)) {
      header_of(dp)->refs.fetch_add(1, std::memory_order_relaxed);
   }
   return dp;
}

/**
 * @brief Give up one reference on a descriptor
 *
 * The release of the last reference removes the descriptor from the registry,
 * unless a lookup has already replaced it there, and frees it. Static
 * descriptors are not counted.
 *
 * @param dpp the descriptor; set to nullptr. May point to nullptr.
 */
void ocs_cull_descr_release(const lDescr **dpp) {
   if (dpp == nullptr || !ocs_cull_descr_is_counted(*dpp)) {
      if (dpp != nullptr) {
         *dpp = nullptr;
      }
      return;
   }

   cull_descr_header *h = header_of(*dpp);
   *dpp = nullptr;
   if (h->refs.fetch_sub(1, std::memory_order_acq_rel) != 1) {
      return;
   }

   cull_descr_registry &reg = registry();
   {
      std::lock_guard<std::mutex> lock(reg.mtx);

      auto [first, last] = reg.map.equal_range(h->hash);
      for (auto it = first; it != last; ++it) {
         if (it->second == h) {
            reg.map.erase(it);
            break;
         }
      }
      if (reg.map.size() < reg.soft_cap) {
         reg.cap_reported = false;
      }
   }
   free_counted(h);
}

/**
 * @brief The reference count of a descriptor
 * @param dp the entries of a descriptor, or nullptr
 * @return the count of a counted descriptor, 0 for any other
 */
uint32_t ocs_cull_descr_refs(const lDescr *dp) {
   if (!ocs_cull_descr_is_counted(dp)) {
      return 0;
   }
   return header_of(dp)->refs.load(std::memory_order_relaxed);
}

/**
 * @brief Number of values an element of a descriptor holds (CS-439, N-B-7)
 *
 * This is the size of `cont` and the number of values on the wire. For a
 * counted descriptor it comes from the header; any other is walked.
 *
 * @param dp the descriptor
 * @return the number of values, or -1 if dp is nullptr
 */
int lCountPresent(const lDescr *dp) {
   if (dp == nullptr) {
      return -1;
   }
   if (ocs_cull_descr_is_counted(dp)) {
      return static_cast<int>(header_of(dp)->np);
   }
   return static_cast<int>(count_present(dp, count_entries(dp)));
}

/**
 * @brief Report a value access outside the slots of an element and abort (CS-439)
 *
 * Called by the debug check of ocs_cull_value(): the position is absent in the
 * element's descriptor, lies outside it, or its slot lies outside the values.
 *
 * @param ep the element
 * @param pos the position that was accessed
 */
void ocs_cull_bad_slot(const lListElem *ep, const int pos) {
   const cull_descr_header *h = ocs_cull_descr_header_of(ep->descr);
   CRITICAL(MSG_CULL_BADSLOT_IIUU, pos, ep->descr[0].nm, h->n, h->np);
   abort();
}

/**
 * @brief Number of registered descriptors
 * @return the size of the registry, without private copies
 */
size_t ocs_cull_descr_registry_size() {
   cull_descr_registry &reg = registry();
   std::lock_guard<std::mutex> lock(reg.mtx);
   return reg.map.size();
}

#ifdef OCS_CULL_STATS
/**
 * @brief The largest number of descriptors the registry held at once
 * @return the peak size of the registry since the process started
 */
size_t ocs_cull_descr_registry_peak() {
   cull_descr_registry &reg = registry();
   std::lock_guard<std::mutex> lock(reg.mtx);
   return reg.peak;
}
#endif

/**
 * @brief The identity of the registry this code uses
 *
 * The address of the registry pointer, in the data of the object holding the
 * code. Two calls return the same value exactly when they use one registry.
 *
 * @return the identity
 */
const void *ocs_cull_descr_registry_id() {
   return &registry_slot();
}

/**
 * @brief Whether a loaded object uses the descriptor registry of this process (N-A-22)
 *
 * An object that links CULL statically carries its own copy of the registry.
 * Its code must still be bound to the copy of the process, which it is when
 * CULL exports its symbols and the loading executable is linked `-rdynamic`.
 * The check calls ocs_cull_descr_registry_seen() of the loaded object, whose
 * call to ocs_cull_descr_registry_id() resolves the way all of its calls into
 * CULL do, and compares the result with the registry of the caller.
 *
 * @param handle the handle dlopen() returned for the object
 * @param[out] own receives the file name of the object holding the registry of the caller
 * @param[out] seen receives the file name of the object holding the registry the loaded object uses, or "none" if
 *             it has no CULL
 * @return true if the loaded object uses the registry of the caller
 */
bool ocs_cull_descr_registry_shared(void *handle, std::string *own, std::string *seen) {
   using seen_func = const void *(*)();
   const void *mine = ocs_cull_descr_registry_id();
   *own = object_of(mine);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
   const auto func = reinterpret_cast<seen_func>(dlsym(handle, "ocs_cull_descr_registry_seen"));
#pragma GCC diagnostic pop
   if (func == nullptr) {
      *seen = "none";
      return false;
   }
   const void *theirs = func();
   *seen = object_of(theirs);
   return theirs == mine;
}

/**
 * @brief Set the soft cap of the registry
 *
 * Meant for tests; the default is #OCS_CULL_DESCR_SOFT_CAP. Lowering the cap
 * does not remove registered descriptors.
 *
 * @param cap the new soft cap
 * @return the previous soft cap
 */
size_t ocs_cull_descr_set_soft_cap(const size_t cap) {
   cull_descr_registry &reg = registry();
   std::lock_guard<std::mutex> lock(reg.mtx);
   const size_t prev = reg.soft_cap;
   reg.soft_cap = cap;
   reg.cap_reported = false;
   return prev;
}
