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
 * @brief Loadable module for test_cull_plugin: a library that links CULL statically, like a spooling plugin
 *
 * Built twice: once as the spooling plugins are, so that its calls into CULL bind
 * to the copy of the loading executable, and once with -Bsymbolic, so that they
 * bind to its own copy and it ends up with a registry of its own.
 */

#include "cull/cull_list.h"
#include "cull/ocs_cull_descr.h"

/**
 * @brief Intern a descriptor through the CULL this module is bound to
 * @param dp the descriptor
 * @return the registered descriptor, holding a reference
 */
extern "C" const lDescr *test_cull_plugin_intern(const lDescr *dp) {
   return ocs_cull_descr_intern(dp);
}

/**
 * @brief Release a descriptor through the CULL this module is bound to
 * @param dpp the descriptor, set to nullptr
 */
extern "C" void test_cull_plugin_release(const lDescr **dpp) {
   ocs_cull_descr_release(dpp);
}

/**
 * @brief The registry this module uses, which also links the probe of the loader check into it
 * @return the identity of the registry
 */
extern "C" const void *test_cull_plugin_probe() {
   return ocs_cull_descr_registry_seen();
}
