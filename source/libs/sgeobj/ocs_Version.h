#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
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

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "cull/cull.h"

namespace ocs {
   class Version {
      static const uint32_t OCS_VERSION;
      static const std::string OCS_VERSION_STRING;
      static const std::string OCS_LONG_PRODUCT_NAME;
      static const std::string OCS_SHORT_PRODUCT_NAME;

      static const std::vector<std::tuple<uint32_t, std::string>> OCS_ALL_VERSIONS_VECTOR;
   public:
      static uint32_t get_version();
      static std::string get_version_string();
      static std::tuple<int, int, int, std::string> get_version_token();

      static std::string get_short_product_name();
      static std::string get_long_product_name();

      static bool do_versions_match(lList **alpp, uint32_t version, const char *host, const char *commproc, int id);
   };
}






