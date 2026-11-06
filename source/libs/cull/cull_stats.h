#pragma once
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
 * @brief Counters on the CULL hash tables, descriptors and packing (CS-2993)
 *
 * Built only with the CMake option `ENABLE_CULL_STATS`, which defines
 * `OCS_CULL_STATS`. Without it the hook macros below expand to nothing, so a
 * default build carries neither the counting code nor its symbols.
 *
 * The counters are kept per thread and merged when a thread ends. At process
 * exit the merged counters are written as one JSON document to the directory named
 * by `OCS_CULL_STATS_DIR`; nothing is written when the variable is unset.
 *
 * A counter is booked either on an object type or on a field:
 * - type counters (#CULL_STATS_TYPE) take the field number of the first field
 *   of a descriptor; the dump resolves it to the type through the name space,
 *   so a reduced descriptor counts towards the type it was reduced from;
 * - field counters (#CULL_STATS_FIELD) take the number of the field itself,
 *   e.g. the field a hash table is built on.
 *
 * The document holds `format`, `program`, `pid`, `written`,
 * `descr_registry_peak` (the largest size the descriptor registry reached) and
 * `threads`, which maps each thread class to `types` (type, then counters) and
 * `fields` (type, then field, then counters). Counters that are 0 are left out.
 *
 * A descriptor copy is every descriptor the library allocates, the same before
 * and after shared descriptors (CS-2997): the `dcopy_*` counters, which are
 * temporary or plain copies, plus `descr_intern_new` and `descr_private`.
 */

#ifdef OCS_CULL_STATS

#include <cstdint>
#include <string>

namespace ocs::cull_stats {
   /** @brief What is counted; the dump names each counter in lower case, e.g. `pos_calls` */
   enum Counter {
      POS_CALLS,        ///< lGetPosInDescr() calls (type)
      POS_REDUCED,      ///< of them on a reduced descriptor, i.e. through the linear search (type)
      POS_ITERS,        ///< descriptor entries visited by that search (type)
      DCOPY_ELEM,       ///< temporary copies made by lCreateElem() to mark every field reduced (type)
      DCOPY_ELEM_B,     ///< their bytes (type)
      DCOPY_OTHER,      ///< descriptor copies made by lCopyDescr() (type)
      DCOPY_OTHER_B,    ///< their bytes (type)
      DCOPY_UNPACK,     ///< temporary descriptors built while unpacking, before the shared one is taken (type)
      DCOPY_UNPACK_B,   ///< their bytes (type)
      HASH_CREATE,      ///< hash tables created (field)
      HASH_BUCKET_B,    ///< bytes of their initial bucket arrays (field)
      HASH_INSERT,      ///< inserts into a hash table (field)
      HASH_LOOKUP,      ///< lookups in a hash table (field)
      HASH_FREE_USED,   ///< hash tables freed after at least one lookup (field)
      HASH_FREE_UNUSED, ///< hash tables freed without ever being looked up (field)
      PACK_DESCR,       ///< descriptors and field selections packed (type)
      PACK_DESCR_B,     ///< their bytes (type)
      PACK_ELEMS,       ///< elements packed (type)
      PACK_VALUE_B,     ///< bytes of packed values, sublists and sub-objects excluded (field)
      UNPACK_LISTS,     ///< lists unpacked (type)
      UNPACK_ELEMS,     ///< their elements (type)
      UNPACK_NS,        ///< nanoseconds spent unpacking them, nested lists included (type)
      UNPACK_HASH_NS,   ///< of that, nanoseconds spent building their hash tables (type); 0 since the tables are built on lookup
      HASH_BUILD_LAZY,  ///< hash tables built and published by a first lookup (field)
      LOOKUP_LINEAR,    ///< lookups on a hashable list searched linearly, as it is below the threshold (field)
      LOOKUP_LEN_LT4,   ///< lookups on a hashable list of fewer than 4 elements (field)
      LOOKUP_LEN_LT8,   ///< ... of 4 to 7 elements (field)
      LOOKUP_LEN_LT16,  ///< ... of 8 to 15 elements (field)
      LOOKUP_LEN_LT32,  ///< ... of 16 to 31 elements (field)
      LOOKUP_LEN_LT64,  ///< ... of 32 to 63 elements (field)
      LOOKUP_LEN_LT128, ///< ... of 64 to 127 elements (field)
      LOOKUP_LEN_GE128, ///< ... of 128 elements or more (field)
      DESCR_INTERN_HIT, ///< descriptors found in the registry (type)
      DESCR_INTERN_NEW, ///< descriptors allocated and registered by a registry miss (type)
      DESCR_INTERN_NEW_B, ///< their bytes, header included (type)
      DESCR_PRIVATE,    ///< counted descriptors allocated outside the registry: private copies, above the soft cap (type)
      DESCR_PRIVATE_B,  ///< their bytes, header included (type)
      DESCR_PACK_TYPE_REF,  ///< full descriptors packed as a reference to their type (type)
      DESCR_PACK_TABLE_DEF, ///< reduced descriptors entered into the descriptor table of a pack buffer (type)
      DESCR_PACK_TABLE_REF, ///< reduced descriptors packed as an index into that table (type)
      NUM_COUNTERS
   };

   /**
    * @brief Add to a counter of an object type, for the calling thread
    * @param nm the field number of the first field of the type's descriptor
    * @param c the counter
    * @param v the amount to add
    */
   void add_type(int nm, Counter c, uint64_t v);

   /**
    * @brief Add to a counter of a field, for the calling thread
    * @param nm the field number
    * @param c the counter
    * @param v the amount to add
    */
   void add_field(int nm, Counter c, uint64_t v);

   /**
    * @brief Read the calling thread's own, not yet merged value of a counter
    * @param type true for a type counter, false for a field counter
    * @param nm the field number the counter was booked on
    * @param c the counter
    * @return the value, 0 if nothing was booked
    */
   uint64_t get(bool type, int nm, Counter c);

   /**
    * @brief Monotonic clock in nanoseconds, for the timing counters
    * @return the current time
    */
   uint64_t now_ns();

   /**
    * @brief Merge the calling thread's counters and write all of them
    *
    * Called at process exit. Writes `<dir>/<program>.<pid>.json`, and a further
    * numbered file if that already exists, which happens when a shared module
    * links its own copy of the library.
    *
    * @param dir the directory to write into
    * @return the path of the file written, empty if nothing was counted or
    *         the file could not be written
    */
   std::string write(const char *dir);
}

/** @brief Book @p v on counter @p c of the object type whose first field is @p nm */
#define CULL_STATS_TYPE(nm, c, v) ocs::cull_stats::add_type((nm), ocs::cull_stats::c, (v))
/** @brief Book @p v on counter @p c of field @p nm */
#define CULL_STATS_FIELD(nm, c, v) ocs::cull_stats::add_field((nm), ocs::cull_stats::c, (v))
/** @brief Compile the code in the argument only in a statistics build */
#define CULL_STATS_CODE(...) __VA_ARGS__

#else

#define CULL_STATS_TYPE(nm, c, v)
#define CULL_STATS_FIELD(nm, c, v)
#define CULL_STATS_CODE(...)

#endif
