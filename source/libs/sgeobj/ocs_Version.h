#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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
 * @brief The product version and its compatibility rules
 */

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "cull/cull.h"

namespace ocs {
   /**
    * @brief The product version, and whether two components may talk to each other
    *
    * Every GDI connection checks the peer's version with #do_versions_match, so
    * a client of a different release is rejected cleanly instead of
    * misinterpreting the data.
    */
   class Version {
   public:
      /**
       * @brief The version as a comparable number
       * @return the encoded version
       */
      static uint32_t get_version();
      /**
       * @brief The version as it is shown to users
       * @return the version string
       */
      static std::string get_version_string();
      /**
       * @brief The version split into its parts
       * @return major, minor, patch level and the release suffix
       */
      static std::tuple<int, int, int, std::string> get_version_token();

      /**
       * @brief The abbreviated product name
       * @return the short name
       */
      static std::string get_short_product_name();
      /**
       * @brief The full product name
       * @return the long name
       */
      static std::string get_long_product_name();

      /// May a peer of this version talk to us?
      static bool do_versions_match(lList **alpp, uint32_t version, const char *host, const char *commproc, int id);
   };
}






