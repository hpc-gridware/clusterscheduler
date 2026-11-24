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
 * @brief The descriptor registry: one shared, reference counted descriptor per reduction
 *
 * A descriptor is either static or counted. A static descriptor is the
 * compile-time block of an object type (ocs::tom::descr_make()); it lives as long as
 * the program, and reference operations on it do nothing. A counted descriptor
 * is allocated together with its #tom_descr_header, carries
 * #MT_DESCR_INTERNED in entry 0 and is freed by the release of its last
 * reference.
 *
 * ocs::tom::descr_intern() returns the registered descriptor equal to its
 * argument, registering a copy on the first call. Equality is decided by the
 * entries (field number, type with flags and slot offset, in order); the hash only selects
 * the candidates. Above the soft cap the registry stops growing and hands out
 * private counted copies, which are correct but not shared.
 *
 * The lists and elements of the library hold their descriptor through
 * ocs::tom::descr_acquire(), which hands out the static block of the type for a
 * full descriptor and the interned one for a reduced descriptor. The static
 * block of a type is found through the installed object model (tom/ocs_Model.h).
 *
 * All functions are thread safe. The registry is consulted only when a
 * descriptor is built, so one mutex is enough.
 */

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "tom/ocs_Types.h"

/**
 * @brief The number of fields a descriptor holds a value for
 * @param dp the descriptor
 * @return the number of present fields, or -1 for nullptr
 */
int lCountPresent(const ocs::tom::FieldDescr *dp);

/** @brief Default number of registered descriptors above which interning falls back to private copies */
#define TOM_DESCR_SOFT_CAP 4096

namespace ocs::tom {
   const ocs::tom::FieldDescr *descr_intern(const ocs::tom::FieldDescr *dp);

   const ocs::tom::FieldDescr *descr_acquire(const ocs::tom::FieldDescr *dp);

   const ocs::tom::FieldDescr *descr_private(const ocs::tom::FieldDescr *dp);

   int descr_layout(const ocs::tom::FieldDescr *src, const std::vector<bool> &keep, ocs::tom::FieldDescr *out);

   const ocs::tom::FieldDescr *descr_build(const ocs::tom::FieldDescr *src, const std::vector<bool> &keep);

   const ocs::tom::FieldDescr *descr_reduce(const ocs::tom::FieldDescr *type, std::initializer_list<int> fields);

   const ocs::tom::FieldDescr *descr_ref(const ocs::tom::FieldDescr *dp);

   void release_descr(const ocs::tom::FieldDescr *&descr) noexcept;

   bool descr_is_counted(const ocs::tom::FieldDescr *dp);

   bool descr_is_static(const ocs::tom::FieldDescr *dp);

   const ocs::tom::FieldDescr *descr_static_for(int nm);

   const ocs::tom::FieldDescr *descr_static_containing(int nm, int *pos);

   uint32_t descr_refs(const ocs::tom::FieldDescr *dp);

   size_t descr_registry_size();

#ifdef OCS_TOM_STATS
   size_t descr_registry_peak();
#endif

   const void *descr_registry_id();

   bool descr_registry_shared(void *handle, std::string &own, std::string &seen);

   size_t descr_set_soft_cap(size_t cap);
} // namespace ocs::tom

extern "C" const void *ocs_tom_descr_registry_seen();
