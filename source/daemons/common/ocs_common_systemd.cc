/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#include "uti/ocs_Systemd.h"

#include "ocs_common_systemd.h"

namespace ocs::common {
   /*!
    * @brief Check if execd should use PDC for usage collection based on the given usage_collection parameter.
    *
    * This function checks the configuration to determine if PDC should be used for usage collection
    * based on the provided usage_collection parameter. It returns true if PDC is enabled or hybrid mode is configured,
    * otherwise false.
    *
    * @param usage_collection The usage collection mode to check against.
    * @return true if PDC is used for usage collection, false otherwise.
    */
   bool
   use_pdc_for_usage_collection(usage_collection_t usage_collection) {
      bool ret = true;

      if (usage_collection == USAGE_COLLECTION_NONE) {
         ret = false; // we do not use PDC for usage collection
      } else {
         // When we are using systemd we usually do not use PDC for usage collection,
         // except when we configured in execd_params USAGE_COLLECTION to use PDC or HYBRID.
#if defined(OCS_WITH_SYSTEMD)
         if (mconf_get_enable_systemd() &&
             ocs::uti::Systemd::is_systemd_available()) {
            ret = false;
            if (usage_collection == USAGE_COLLECTION_PDC || usage_collection == USAGE_COLLECTION_HYBRID) {
               ret = true;
            }
         }
#endif
      }

      return ret;
   }

} // namespace ocs::common
