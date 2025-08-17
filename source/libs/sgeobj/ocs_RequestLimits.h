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

#include <string>
#include <ostream>
#include <vector>
#include <unordered_map>
#include <pthread.h>

#include "cull/cull.h"

#include "gdi/ocs_gdi_Packet.h"

using namespace std;

namespace ocs {
   /** @brief A singleton class to manage all request limit rules.
    *
    * The class is a singleton to ensure that all request limit rules are managed in one place.
    * The class provides methods to parse the request limit string from the configuration
    * and to check if a new request would exceed the defined limits.
    */
   class RequestLimits {
      static pthread_mutex_t mutex; ///< mutex to protect the singleton instance
      static RequestLimits *instance; ///< the one and only instance
      RequestLimits() = default; ///< private constructor to prevent instantiation

   public:
      static RequestLimits &get_instance();

      bool parse(const std::string &request_limit_string, lList **answer_list);
      bool parse_from_config(lList **answer_list);
      bool will_exceed_limit(gdi::Packet *packet, gdi::Task *task, lList **answer_list);
   };
}
