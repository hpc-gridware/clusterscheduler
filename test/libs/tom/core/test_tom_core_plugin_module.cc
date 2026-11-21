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
 * @brief Loadable module for test_tom_core_plugin: a library that links CULL statically, like a spooling plugin
 *
 * Built twice: once as the spooling plugins are, so that its calls into CULL bind
 * to the copy of the loading executable, and once with -Bsymbolic, so that they
 * bind to its own copy and it ends up with a registry of its own.
 */

#include "tom/ocs_Types.h"
#include "tom/ocs_Descr.h"

/**
 * @brief Intern a descriptor through the CULL this module is bound to
 * @param dp the descriptor
 * @return the registered descriptor, holding a reference
 */
extern "C" const lDescr *test_tom_core_plugin_intern(const lDescr *dp) {
   return ocs::tom::descr_intern(dp);
}

/**
 * @brief Release a descriptor through the CULL this module is bound to
 * @param dpp the holder of the descriptor, set to nullptr; nullptr is ignored
 */
extern "C" void test_tom_core_plugin_release(const lDescr **dpp) {
   if (dpp != nullptr) {
      ocs::tom::release_descr(*dpp);
   }
}

/**
 * @brief The registry this module uses
 *
 * The probe of the loader check itself is linked by ocs_link_registry_probe(), as in a spooling plugin; this
 * function only asks the registry identity for the test.
 *
 * @return the identity of the registry
 */
extern "C" const void *test_tom_core_plugin_probe() {
   return ocs::tom::descr_registry_id();
}
