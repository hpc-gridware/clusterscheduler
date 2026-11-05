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
 * compile-time block of an object type (LISTDEF/LISTEND); it lives as long as
 * the program, and reference operations on it do nothing. A counted descriptor
 * is allocated together with its #cull_descr_header, carries
 * #CULL_DESCR_INTERNED in entry 0 and is freed by the release of its last
 * reference.
 *
 * ocs_cull_descr_intern() returns the registered descriptor equal to its
 * argument, registering a copy on the first call. Equality is decided by the
 * entries (field number and type with flags, in order); the hash only selects
 * the candidates. Above the soft cap the registry stops growing and hands out
 * private counted copies, which are correct but not shared.
 *
 * The lists and elements of the library hold their descriptor through
 * ocs_cull_descr_acquire(), which hands out the static block of the type for a
 * full descriptor and the interned one for a reduced descriptor. The static
 * block of a type is found through the name space of the calling thread.
 *
 * All functions are thread safe. The registry is consulted only when a
 * descriptor is built, so one mutex is enough.
 */

#include <cstddef>
#include <cstdint>

#include "cull/cull_list.h"

/** @brief Default number of registered descriptors above which interning falls back to private copies */
#define OCS_CULL_DESCR_SOFT_CAP 4096

const lDescr *ocs_cull_descr_intern(const lDescr *dp);

const lDescr *ocs_cull_descr_acquire(const lDescr *dp);

const lDescr *ocs_cull_descr_private(const lDescr *dp);

const lDescr *ocs_cull_descr_ref(const lDescr *dp);

void ocs_cull_descr_release(const lDescr **dpp);

bool ocs_cull_descr_is_counted(const lDescr *dp);

bool ocs_cull_descr_is_static(const lDescr *dp);

uint32_t ocs_cull_descr_refs(const lDescr *dp);

size_t ocs_cull_descr_registry_size();

size_t ocs_cull_descr_set_soft_cap(size_t cap);
