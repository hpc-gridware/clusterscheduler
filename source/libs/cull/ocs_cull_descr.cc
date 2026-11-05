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
#include <mutex>
#include <new>
#include <unordered_map>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_list.h"
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
   };

   /**
    * @brief The one registry of the process
    *
    * Allocated on first use and never destroyed, so that a descriptor released
    * by a thread that outlives the static destructors still finds it.
    *
    * @return the registry
    */
   cull_descr_registry &registry() {
      static auto *r = new cull_descr_registry;
      return *r;
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
    * @return true if every entry has the same field number and type
    */
   bool same_entries(const lDescr *a, const lDescr *b, const uint32_t n) {
      for (uint32_t i = 0; i < n; i++) {
         if (a[i].nm != b[i].nm || (a[i].mt & ~CULL_DESCR_INTERNED) != (b[i].mt & ~CULL_DESCR_INTERNED)) {
            return false;
         }
      }
      return true;
   }

   /**
    * @brief Allocate a counted copy of a descriptor, holding one reference
    * @param dp the entries to copy
    * @param n their number, without the terminating one
    * @param hash their hash
    * @return the header of the copy, or nullptr if out of memory
    */
   cull_descr_header *alloc_counted(const lDescr *dp, const uint32_t n, const uint64_t hash) {
      void *mem = sge_malloc(sizeof(cull_descr_header) + (n + 1) * sizeof(lDescr));
      if (mem == nullptr) {
         return nullptr;
      }
      auto *h = new (mem) cull_descr_header{1, n, hash};
      lDescr *e = entries_of(h);
      memcpy(e, dp, (n + 1) * sizeof(lDescr));
      e[0].mt |= CULL_DESCR_INTERNED;
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
               DRETURN(entries_of(h));
            }
            // dead, its releasing thread frees it; replace the entry
            fresh = alloc_counted(dp, n, hash);
            if (fresh == nullptr) {
               DRETURN(nullptr);
            }
            it->second = fresh;
            DRETURN(entries_of(fresh));
         }
      }

      fresh = alloc_counted(dp, n, hash);
      if (fresh == nullptr) {
         DRETURN(nullptr);
      }
      if (reg.map.size() < reg.soft_cap) {
         reg.map.emplace(hash, fresh);
         DRETURN(entries_of(fresh));
      }
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
 * @brief Number of registered descriptors
 * @return the size of the registry, without private copies
 */
size_t ocs_cull_descr_registry_size() {
   cull_descr_registry &reg = registry();
   std::lock_guard<std::mutex> lock(reg.mtx);
   return reg.map.size();
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
