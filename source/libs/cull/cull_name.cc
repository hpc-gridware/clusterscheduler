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

#include <cstdio>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_rmon_macros.h"

#include "cull/cull_list.h"
#include "cull/cull_state.h"
#include "cull/cull_lerrnoP.h"
#include "cull/cull_name.h"

#ifdef OBSERVE
#  include "cull/cull_observe.h"
#endif

#define CULL_BASIS_LAYER CULL_LAYER

namespace {

/** @brief Index one name space entry: return the name for @p nm if in range.
 *  @param nsp  one name space entry (may be null)
 *  @param nm   CULL name id
 *  @return     the name, or nullptr if @p nm is not within this entry */
const char *_lNm2Str(const lNameSpace *nsp, const int nm) {
   DENTER(CULL_BASIS_LAYER);

   if (!nsp) {
      DPRINTF("name vector uninitialized !!\n");
      DRETURN(nullptr);
   }

   if (nm >= nsp->lower && nm < (nsp->lower + nsp->size)) {
      DRETURN(nsp->namev[nm - nsp->lower]);
   }

   DRETURN(nullptr);
}

/** @brief Transparent hash for string_view keys so lookups need no temporary
 *         std::string. Keys are string_views onto the static namev strings,
 *         which live as long as the name space (process lifetime for the only
 *         name space, nmv), so no copies are made and the views never dangle.
 */
struct sv_hash {
   size_t operator()(std::string_view s) const noexcept {
      return std::hash<std::string_view>{}(s);
   }
};
using NameIndex = std::unordered_map<std::string_view, int, sv_hash, std::equal_to<>>;

/** @brief Build the Name->id map for one name space.
 *
 * Entries and indices are walked in the same order the historical linear scan
 * used, and emplace keeps the first insertion, so first-match-wins semantics
 * are preserved exactly.
 *
 * @param ns  sentinel-terminated name space array (lower==0 ends it)
 * @return    map from field name to its CULL name id
 */
NameIndex build_name_index(const lNameSpace *ns) {
   NameIndex m;
   for (const lNameSpace *e = ns; e->lower; ++e) {
      for (int i = 0; i < e->size; ++i) {
         if (e->namev[i] != nullptr) {
            m.emplace(e->namev[i], e->lower + i);
         }
      }
   }
   return m;
}

/** @brief Immutable {name space, index} pair, published as a single unit. */
struct CacheEntry {
   const lNameSpace *ns;    ///< name space this entry resolves
   const NameIndex *idx;    ///< its build-once Name->id index
};

// Process-wide cache of per-name-space indices. A name space is immutable once
// built, so each index is built once and then read without a lock.
std::mutex name_index_mtx;                              ///< guards name_index_map during a build
std::unordered_map<const lNameSpace *, std::unique_ptr<const NameIndex>> name_index_map; ///< per-name-space build-once indices
std::atomic<const CacheEntry *> last_cache{nullptr};  ///< last-used pair; lock-free fast path

/** @brief Return the build-once, immutable Name->id index for a name space.
 *
 * The mutex is taken only on the first lookup for a given name space (in
 * practice once, for the single global nmv); subsequent calls hit the lock-free
 * fast path. The cached {ns,idx} pair is published as a SINGLE atomic pointer to
 * an immutable CacheEntry, so a reader always observes a consistent pair -- two
 * separate atomics could tear (return one name space's index for another) once
 * more than one name space is in play.
 *
 * @param ns  the bound or explicitly passed name space to resolve against
 * @return    pointer to the immutable index for @p ns (never null)
 */
const NameIndex *name_index(const lNameSpace *ns) {
   // Fast path: one acquire load yields a consistent {ns,idx} snapshot.
   if (const CacheEntry *cached = last_cache.load(std::memory_order_acquire); cached != nullptr && cached->ns == ns) {
      return cached->idx;
   }
   std::lock_guard<std::mutex> guard(name_index_mtx);
   auto &slot = name_index_map[ns];
   if (!slot) {
      slot = std::make_unique<const NameIndex>(build_name_index(ns));
   }
   const NameIndex *idx = slot.get();
   // Publish a fresh immutable pair. The previous CacheEntry is intentionally
   // leaked: at most one per distinct name space (one in practice, for nmv), so
   // it never accumulates, and not freeing it avoids a use-after-free of a
   // pointer a concurrent fast-path reader may still be dereferencing.
   last_cache.store(new CacheEntry{ns, idx}, std::memory_order_release);
   return idx;
}

}  // namespace

/**
 * @brief Return the string name for a CULL name id.
 *
 * Scans the bound name space's entries and indexes directly within the matching
 * entry (namev[nm - lower]) -- O(number of name-space entries), O(1) per entry.
 * On an unknown id or an unbound name space it records the index via
 * cull_state_set_noinit() and returns that placeholder string.
 *
 * @param nm  CULL name id (e.g. CONF_name)
 * @return    the field name, or a "Nameindex = N" placeholder if unknown
 */
const char *lNm2Str(const int nm) {
   DENTER(CULL_BASIS_LAYER);

   const lNameSpace *ns;
   if (!((ns = cull_state_get_name_space()))) {
      DPRINTF("name vector uninitialized !!\n");
      goto Error;
   }

   for (const lNameSpace *nsp = ns; nsp->lower; nsp++) {
      const char *cp;
      if ((cp = _lNm2Str(nsp, nm))) {
         DRETURN(cp);
      }
   }

Error:
   char stack_no_init[50];
   snprintf(stack_no_init, sizeof(stack_no_init), "Nameindex = %d", nm);
   cull_state_set_noinit(stack_no_init);
   LERROR(LENAMENOT);
   DRETURN(cull_state_get_noinit());
}

/**
 * @brief Return the integer name id for a field name string.
 *
 * Resolution uses a build-once, then immutable per-name-space hash (see
 * name_index()), so each call is O(1) instead of the historical linear strcmp
 * scan. The hash is keyed on the lNameSpace* that is bound/passed, never on a
 * concrete name space symbol, to keep the cull layer free of sgeobj
 * dependencies. First-match and NoName semantics are preserved.
 *
 * @param str  field name to resolve
 * @param ns   name space to resolve against; nullptr uses the calling thread's
 *             bound default name space (cull_state_get_name_space())
 * @return     the CULL name id, or NoName if @p str is unknown or no name
 *             space is available
 */
int lStr2Nm(const char *str, const lNameSpace *ns) {
   DENTER(CULL_BASIS_LAYER);

   if (str == nullptr) {
      DRETURN(NoName);
   }

   // use the default CULL namespace if nothing is provided by the caller
   if (ns == nullptr) {
      ns = cull_state_get_name_space();
      if (ns == nullptr) {
         DRETURN(NoName);
      }
   }

   const NameIndex *idx = name_index(ns);
   if (const auto it = idx->find(std::string_view{str}); it != idx->end()) {
      DRETURN(it->second);
   }

   LERROR(LENAMENOT);
   DRETURN(NoName);
}

/**
 * @brief Resolve @p str against the calling thread's default name space.
 * @param str  field name to resolve
 * @return     the CULL name id, or NoName if unknown
 */
int lStr2Nm(const char *str) {
   return lStr2Nm(str, nullptr);
}

/**
 * @brief Bind the CULL name space used by lNm2Str()/lStr2Nm() on this thread.
 * @param ns_vector  sentinel-terminated name space array
 */
void lInit(const lNameSpace *ns_vector) {
   cull_state_set_name_space(ns_vector);
#ifdef OBSERVE
   lObserveInit();
#endif
}
