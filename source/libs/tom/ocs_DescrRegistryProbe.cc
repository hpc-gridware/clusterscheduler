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
 * @brief The probe a loaded object answers the descriptor registry check with (N-A-22)
 *
 * Kept in a translation unit of its own, apart from the registry in
 * ocs_cull_descr.cc: in a loaded object that holds its own copy of the static
 * library, the probe's call of ocs::tom::descr_registry_id() then goes through the
 * dynamic linker and returns the registry the object's code is bound to. Nothing
 * in a loadable object calls the probe, so the static library would not link this
 * unit into it; every loadable object therefore links it explicitly with
 * ocs_link_registry_probe() (cmake/OcsRegistryProbe.cmake).
 */

#include "tom/ocs_cull_descr.h"

/**
 * @brief The descriptor registry the code of this object uses
 *
 * Exported with C linkage for dlsym(), see ocs::tom::descr_registry_shared().
 *
 * @return the identity of that registry, see ocs::tom::descr_registry_id()
 */
extern "C" const void *ocs_tom_descr_registry_seen() {
   return ocs::tom::descr_registry_id();
}
